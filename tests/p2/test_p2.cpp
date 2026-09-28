// tests/p2/test_p2.cpp
//
// Project 2 test suite.
// Current coverage: Message / Conversation / SentinelScanner / Harness integration.

#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/scripted_client.h"
#include "model/replay_client.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

// ------------------------------------------------------------
// Conversation tests
// ------------------------------------------------------------

void test_empty_conversation() {
    Conversation conv;

    assert(conv.size() == 0);
    assert(conv.begin() == conv.end());

    bool threw = false;

    try {
        (void)conv.at(0);
    }
    catch (const std::out_of_range&) {
        threw = true;
    }

    assert(threw);
}

void test_append_and_access() {
    Conversation conv;

    conv.append(Message(Role::User, "hello"));
    conv.append(Message(Role::Assistant, "Hi!"));

    assert(conv.size() == 2);

    assert(conv.at(0).role() == Role::User);
    assert(conv.at(0).content() == "hello");

    assert(conv.at(1).role() == Role::Assistant);
    assert(conv.at(1).content() == "Hi!");
}

void test_growth_preserves_messages() {
    Conversation conv;

    // Forces multiple reallocations: 1, 2, 4, 8, 16, ...
    for (int i = 0; i < 100; ++i) {
        conv.append(
            Message(Role::User, "message " + std::to_string(i))
        );
    }

    assert(conv.size() == 100);

    for (int i = 0; i < 100; ++i) {
        assert(
            conv.at(static_cast<std::size_t>(i)).content()
            == "message " + std::to_string(i)
        );
    }
}

void test_doubling_growth() {
    Conversation conv;

    // First append allocates capacity 1.
    conv.append(Message(Role::User, "0"));
    const Message* capacity_1 = conv.begin();
    assert(conv.size() == 1);

    // Second append must grow 1 -> 2.
    conv.append(Message(Role::User, "1"));
    const Message* capacity_2 = conv.begin();
    assert(capacity_2 != capacity_1);
    assert(conv.size() == 2);

    // Third append must grow 2 -> 4.
    conv.append(Message(Role::User, "2"));
    const Message* capacity_4 = conv.begin();
    assert(capacity_4 != capacity_2);
    assert(conv.size() == 3);

    // Fourth append still fits in capacity 4, so no reallocation.
    conv.append(Message(Role::User, "3"));
    assert(conv.begin() == capacity_4);
    assert(conv.size() == 4);

    // Fifth append must grow 4 -> 8.
    conv.append(Message(Role::User, "4"));
    assert(conv.begin() != capacity_4);
    assert(conv.size() == 5);

    for (std::size_t i = 0; i < conv.size(); ++i) {
        assert(conv.at(i).content() == std::to_string(i));
    }
}


void test_iteration_order() {
    Conversation conv;

    conv.append(Message(Role::System, "system"));
    conv.append(Message(Role::User, "user"));
    conv.append(Message(Role::Assistant, "assistant"));

    const Role expected[] = {
        Role::System,
        Role::User,
        Role::Assistant
    };

    std::size_t i = 0;

    for (const Message& message : conv) {
        assert(i < 3);
        assert(message.role() == expected[i]);
        ++i;
    }

    assert(i == 3);
}

void test_copy_constructor_deep_copy() {
    Conversation original;

    original.append(Message(Role::User, "hello"));
    original.append(Message(Role::Assistant, "Hi!"));

    Conversation copy(original);

    assert(copy.size() == original.size());

    // Deep copy means different backing arrays.
    assert(copy.begin() != original.begin());

    assert(copy.at(0).role() == original.at(0).role());
    assert(copy.at(0).content() == original.at(0).content());

    assert(copy.at(1).role() == original.at(1).role());
    assert(copy.at(1).content() == original.at(1).content());
}

void test_copy_assignment_deep_copy() {
    Conversation source;
    source.append(Message(Role::User, "source"));

    Conversation destination;
    destination.append(Message(Role::Assistant, "old data"));

    destination = source;

    assert(destination.size() == 1);
    assert(destination.at(0).role() == Role::User);
    assert(destination.at(0).content() == "source");

    // Copy assignment must also create independent storage.
    assert(destination.begin() != source.begin());
}

void test_move_constructor() {
    Conversation source;

    source.append(Message(Role::User, "hello"));
    source.append(Message(Role::Assistant, "world"));

    const Message* old_pointer = source.begin();

    Conversation destination(std::move(source));

    assert(destination.size() == 2);

    // Move constructor steals the same backing array.
    assert(destination.begin() == old_pointer);

    // Source must be valid and empty afterward.
    assert(source.size() == 0);
    assert(source.begin() == source.end());
}

void test_move_assignment() {
    Conversation source;
    source.append(Message(Role::User, "new data"));

    const Message* old_pointer = source.begin();

    Conversation destination;
    destination.append(Message(Role::Assistant, "old data"));

    destination = std::move(source);

    assert(destination.size() == 1);
    assert(destination.at(0).role() == Role::User);
    assert(destination.at(0).content() == "new data");

    // Exact allocation should have been stolen.
    assert(destination.begin() == old_pointer);

    // Moved-from source must remain valid and empty.
    assert(source.size() == 0);
    assert(source.begin() == source.end());
}

// ------------------------------------------------------------
// SentinelScanner tests
// ------------------------------------------------------------

void test_scanner_whole_sentinel() {
    const std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);

    auto out = scanner.feed("Goodbye.<|end_conversation|>");

    assert(out.safe_text == "Goodbye.");
    assert(out.sentinel_found);
}

void test_scanner_every_split_boundary() {
    const std::string sentinel = "<|end_conversation|>";
    const std::string text = "Goodbye." + sentinel;

    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(sentinel);

        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));

        assert(out1.sentinel_found || out2.sentinel_found);
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }
}

void test_scanner_one_character_at_a_time() {
    const std::string sentinel = "<|end_conversation|>";
    const std::string text = "Goodbye." + sentinel;

    SentinelScanner scanner(sentinel);

    std::string emitted;
    bool found = false;

    for (char c : text) {
        std::string chunk(1, c);

        auto out = scanner.feed(chunk);
        emitted += out.safe_text;

        if (out.sentinel_found) {
            found = true;
            break;
        }
    }

    assert(found);
    assert(emitted == "Goodbye.");
}

void test_scanner_false_alarm_and_flush() {
    const std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);

    auto out1 = scanner.feed("Hello <|end_conversati");
    auto out2 = scanner.feed("XYZ");

    assert(!out1.sentinel_found);
    assert(!out2.sentinel_found);

    auto final = scanner.flush();

    assert(!final.sentinel_found);

    const std::string emitted =
        out1.safe_text +
        out2.safe_text +
        final.safe_text;

    assert(emitted == "Hello <|end_conversatiXYZ");
}



void test_scanner_bounded_memory() {
    const std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);

    // 4 MB adversarial stream, delivered one byte at a time.
    // Repeating a sentinel-like prefix forces the scanner to keep
    // reconsidering possible partial matches without ever completing one.
    const std::string pattern = "<|end_";
    const std::size_t stream_size = 4u * 1024u * 1024u;

    std::size_t emitted_count = 0;

    for (std::size_t i = 0; i < stream_size; ++i) {
        const char c = pattern[i % pattern.size()];
        const std::string chunk(1, c);

        auto out = scanner.feed(chunk);

        assert(!out.sentinel_found);
        emitted_count += out.safe_text.size();

        // Everything fed but not yet emitted is still being withheld.
        // That amount must never exceed sentinel.size() - 1.
        const std::size_t fed_count = i + 1;
        const std::size_t withheld_count = fed_count - emitted_count;

        assert(withheld_count <= sentinel.size() - 1);
    }

    // End-of-stream releases every byte that was still withheld.
    auto final = scanner.flush();
    assert(!final.sentinel_found);

    emitted_count += final.safe_text.size();
    assert(emitted_count == stream_size);
}

// ------------------------------------------------------------
// Harness integration tests
// ------------------------------------------------------------

class TestInput : public InputSource {
public:
    explicit TestInput(std::string line)
        : line_(std::move(line)) {}

    std::string read_line() override {
        if (used_) {
            eof_ = true;
            return "";
        }

        used_ = true;
        return line_;
    }

    bool is_eof() const override {
        return eof_;
    }

private:
    std::string line_;
    bool used_ = false;
    bool eof_ = false;
};


class TestOutput : public OutputSink {
public:
    void write(std::string_view text) override {
        text_ += text;
    }

    const std::string& text() const noexcept {
        return text_;
    }

private:
    std::string text_;
};


void write_test_script(const std::string& path, const std::string& contents) {
    std::ofstream file(path);
    assert(file.is_open());
    file << contents;
}


void test_harness_turn_limit() {
    const std::string path = "p2_turn_limit_test.script";

    write_test_script(
        path,
        "role: assistant\n"
        "Still here.\n"
    );

    HarnessConfig config;
    config.max_turns = 1;

    auto model = std::make_unique<ScriptedModelClient>(path);
    config.system_message = model->system_message();

    Harness harness(std::move(model), config);
    TestInput input("hello");
    TestOutput output;

    StopReason reason = harness.run(input, output);

    assert(reason.kind == StopReason::Kind::TurnLimit);
    assert(output.text().find("Still here.") != std::string::npos);

    std::remove(path.c_str());
}


void test_harness_sentinel_halt() {
    const std::string path = "p2_sentinel_test.script";

    // chunk: 1 forces the stop sentinel to arrive one character at a time.
    write_test_script(
        path,
        "chunk: 1\n"
        "role: assistant\n"
        "Goodbye.<|end_conversation|>\n"
    );

    HarnessConfig config;
    config.max_turns = 10;

    auto model = std::make_unique<ScriptedModelClient>(path);
    config.system_message = model->system_message();

    Harness harness(std::move(model), config);
    TestInput input("bye");
    TestOutput output;

    StopReason reason = harness.run(input, output);

    assert(reason.kind == StopReason::Kind::Sentinel);
    assert(output.text().find("Goodbye.") != std::string::npos);
    assert(output.text().find("<|end_conversation|>") == std::string::npos);

    std::remove(path.c_str());
}




void test_system_message_ordering() {
    const std::string path = "p2_system_order_test.script";

    write_test_script(
        path,
        "role: assistant\n"
        "Acknowledged.\n"
    );

    HarnessConfig config;
    config.max_turns = 1;
    config.system_message = "Be concise.";

    auto model = std::make_unique<ScriptedModelClient>(path);
    Harness harness(std::move(model), config);
    TestInput input("hello");
    TestOutput output;

    StopReason reason = harness.run(input, output);
    assert(reason.kind == StopReason::Kind::TurnLimit);

    const Conversation& conv = harness.conversation();
    assert(conv.size() == 3);
    assert(conv.at(0).role() == Role::System);
    assert(conv.at(0).content() == "Be concise.");
    assert(conv.at(1).role() == Role::User);
    assert(conv.at(2).role() == Role::Assistant);

    std::remove(path.c_str());
}


void test_harness_eof() {
    const std::string path = "p2_eof_test.script";

    write_test_script(
        path,
        "role: assistant\n"
        "First reply.\n"
    );

    HarnessConfig config;
    config.max_turns = 10;

    auto model = std::make_unique<ScriptedModelClient>(path);
    config.system_message = model->system_message();

    Harness harness(std::move(model), config);

    // TestInput supplies one user line, then reports EOF on the next read.
    TestInput input("hello");
    TestOutput output;

    StopReason reason = harness.run(input, output);

    assert(reason.kind == StopReason::Kind::UserExit);

    // The completed first turn remains intact; EOF must not append
    // a spurious empty user message.
    const Conversation& conv = harness.conversation();
    assert(conv.size() == 2);
    assert(conv.at(0).role() == Role::User);
    assert(conv.at(0).content() == "hello");
    assert(conv.at(1).role() == Role::Assistant);
    assert(conv.at(1).content() == "First reply.");

    std::remove(path.c_str());
}


class CaptureTokenSink : public TokenSink {
public:
    void on_chunk(std::string_view chunk) override {
        text_ += chunk;
    }

    void on_complete() override {
        complete_ = true;
    }

    const std::string& text() const noexcept {
        return text_;
    }

    bool complete() const noexcept {
        return complete_;
    }

private:
    std::string text_;
    bool complete_ = false;
};


const char* test_role_name(Role role) {
    switch (role) {
        case Role::System: return "system";
        case Role::User: return "user";
        case Role::Assistant: return "assistant";
    }

    return "assistant";
}


void save_mock_transcript(const Conversation& conv, const std::string& path) {
    std::ofstream file(path);
    assert(file.is_open());

    bool first = true;

    for (const Message& message : conv) {
        if (!first) {
            file << "---\n";
        }

        first = false;
        file << "role: " << test_role_name(message.role()) << "\n";
        file << message.content() << "\n";
    }
}


void test_replay_round_trip() {
    const std::string path = "p2_replay_round_trip_test.txt";

    // Build the original conversation that will be serialized.
    Conversation original;
    original.append(Message(Role::System, "Be concise."));
    original.append(Message(Role::User, "hello"));
    original.append(Message(Role::Assistant, "Hi!"));
    original.append(Message(Role::User, "goodbye"));
    original.append(Message(Role::Assistant,
                            "Goodbye.<|end_conversation|>"));

    save_mock_transcript(original, path);

    // ReplayModelClient should reproduce assistant turns verbatim.
    ReplayModelClient replay(path);

    Conversation context;
    context.append(Message(Role::System, "Be concise."));
    context.append(Message(Role::User, "hello"));

    CaptureTokenSink first_reply;
    replay.generate(context, first_reply);

    assert(first_reply.complete());
    assert(first_reply.text() == original.at(2).content());

    context.append(Message(Role::Assistant, first_reply.text()));
    context.append(Message(Role::User, "goodbye"));

    CaptureTokenSink second_reply;
    replay.generate(context, second_reply);

    assert(second_reply.complete());
    assert(second_reply.text() == original.at(4).content());

    std::remove(path.c_str());
}

// ------------------------------------------------------------
// Run current P2 tests
// ------------------------------------------------------------

int main() {
    // Conversation
    test_empty_conversation();
    test_append_and_access();
    test_growth_preserves_messages();
    test_doubling_growth();
    test_iteration_order();
    test_copy_constructor_deep_copy();
    test_copy_assignment_deep_copy();
    test_move_constructor();
    test_move_assignment();

    // SentinelScanner
    test_scanner_whole_sentinel();
    test_scanner_every_split_boundary();
    test_scanner_one_character_at_a_time();
    test_scanner_false_alarm_and_flush();
    test_scanner_bounded_memory();

    // Harness integration
    test_harness_turn_limit();
    test_harness_sentinel_halt();
    test_system_message_ordering();
    test_harness_eof();

    // Replay round-trip
    test_replay_round_trip();

    std::cout << "All current P2 tests passed.\n";

    return 0;
}
