#include "TaikoResultArchive.h"
#include <charconv>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>

namespace
{
    constexpr std::string_view Header = "taiko-result";
    // Version 2 adds each group's combo-counted notes. Version 1 details lived only in record databases
    // before schema 4, which are set aside unread, so they are not read.
    constexpr std::uint64_t Version = 2;
    // Bounds every stored list so a damaged count fails instead of asking for huge memory.
    constexpr std::uint64_t MaximumItems = 50'000'000;

    // Space-separated words, one line per part, so a saved detail stays readable.
    class Writer
    {
      public:
        Writer &Word(std::string_view word)
        {
            if (!text_.empty() && text_.back() != '\n')
                text_ += ' ';
            text_ += word;
            return *this;
        }
        Writer &Integer(std::int64_t value)
        {
            return Word(std::to_string(value));
        }
        Writer &Count(std::uint64_t value)
        {
            return Word(std::to_string(value));
        }
        // The shortest text that reads back as the same double.
        Writer &Number(double value)
        {
            return Word(std::format("{}", value));
        }
        Writer &Optional(const std::optional<double> &value)
        {
            return value ? Number(*value) : Word("-");
        }
        void EndLine()
        {
            text_ += '\n';
        }
        [[nodiscard]] std::string Take()
        {
            return std::move(text_);
        }

      private:
        std::string text_;
    };

    class Reader
    {
      public:
        explicit Reader(std::string_view text) : text_(text)
        {
        }
        [[noreturn]] static void Fail()
        {
            throw std::runtime_error("Damaged result detail.");
        }
        std::string_view Token()
        {
            SkipSpace();
            const auto start = position_;
            while (position_ < text_.size() && !IsSpace(text_[position_]))
                ++position_;
            if (position_ == start)
                Fail();
            return text_.substr(start, position_ - start);
        }
        void Expect(std::string_view word)
        {
            if (Token() != word)
                Fail();
        }
        std::uint64_t Count(std::uint64_t maximum = MaximumItems)
        {
            const auto value = Parse<std::uint64_t>(Token());
            if (value > maximum)
                Fail();
            return value;
        }
        std::size_t Size()
        {
            return static_cast<std::size_t>(Count(std::numeric_limits<std::uint64_t>::max()));
        }
        bool Flag()
        {
            return Count(1) != 0;
        }
        std::int64_t Integer()
        {
            return Parse<std::int64_t>(Token());
        }
        double Number()
        {
            return Finite(Parse<double>(Token()));
        }
        std::optional<double> Optional()
        {
            const auto token = Token();
            if (token == "-")
                return std::nullopt;
            return Finite(Parse<double>(token));
        }
        // A list size, or none when the part was not recorded ("-").
        std::optional<std::uint64_t> OptionalCount()
        {
            const auto token = Token();
            if (token == "-")
                return std::nullopt;
            const auto value = Parse<std::uint64_t>(token);
            if (value > MaximumItems)
                Fail();
            return value;
        }
        AccuracyScore Accuracy()
        {
            return AccuracyScore::FromUnits(static_cast<std::uint32_t>(Count(AccuracyScore::MaximumUnits)));
        }
        void End()
        {
            SkipSpace();
            if (position_ != text_.size())
                Fail();
        }

      private:
        static bool IsSpace(char c) noexcept
        {
            return std::string_view(" \n\r\t").find(c) != std::string_view::npos;
        }
        void SkipSpace() noexcept
        {
            while (position_ < text_.size() && IsSpace(text_[position_]))
                ++position_;
        }
        static double Finite(double value)
        {
            if (!std::isfinite(value))
                Fail();
            return value;
        }
        template <typename T> static T Parse(std::string_view token)
        {
            T value{};
            const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
            if (error != std::errc{} || end != token.data() + token.size())
                Fail();
            return value;
        }

        std::string_view text_;
        std::size_t position_{};
    };
} // namespace

std::string SaveTaikoResultDetail(const TaikoPlayResult &result)
{
    const auto &s = result.summary;
    Writer w;
    w.Word(Header).Count(Version).EndLine();
    w.Word("end").Count(static_cast<std::uint64_t>(s.endReason)).Integer(s.initialTime.count());
    w.Integer(s.endTime.count()).Count(s.unprocessedNotes).EndLine();
    w.Word("judgements");
    for (const auto count : result.judgements)
        w.Count(count);
    w.Count(result.okCount).Count(result.poorCount).EndLine();
    w.Word("ticks").Count(result.acceptedTicks).Count(result.missedTicks).EndLine();
    w.Word("groups");
    for (const auto &group : result.groups)
        w.Count(group.total).Count(group.processed).Count(group.accuracy.Units()).Count(group.succeeded);
    w.EndLine();
    w.Word("timing").Optional(result.meanErrorMilliseconds).Optional(result.varianceMillisecondsSquared);
    w.Optional(result.standardDeviationMilliseconds).EndLine();
    w.Word("controls").Count(s.controls.size());
    for (const auto &control : s.controls)
    {
        w.Count(static_cast<std::uint64_t>(control.type)).Integer(control.from.count()).Integer(control.to.count());
        w.Count(control.inputIndex).Count(control.pauseMenuShown);
    }
    w.EndLine();
    w.Word("gauges").Count(s.hpGauges.size());
    for (const auto &gauge : s.hpGauges)
        w.Number(gauge.percent).Count(gauge.applicable).Count(gauge.passed).Number(gauge.clearPercent);
    w.EndLine();
    w.Word("history");
    if (s.hpHistory)
    {
        w.Count(s.hpHistory->size());
        for (const auto &point : *s.hpHistory)
            w.Integer(point.time.count()).Number(point.percent);
    }
    else
        w.Word("-");
    w.EndLine();
    w.Word("errors").Count(result.timingErrors.size());
    for (const auto &sample : result.timingErrors)
    {
        w.Count(sample.noteId).Count(sample.hitIndex).Integer(sample.inputTime.count()).Integer(sample.error.count());
        w.Count(static_cast<std::uint64_t>(sample.grade)).Count(sample.accuracy.Units());
    }
    w.EndLine();
    return w.Take();
}

std::unique_ptr<TaikoPlayResult> LoadTaikoResultDetail(PlayResultSummary summary, std::string_view detail)
{
    Reader r(detail);
    r.Expect(Header);
    if (r.Count(std::numeric_limits<std::uint64_t>::max()) != Version)
        throw std::runtime_error("Unknown result detail version.");
    auto result = std::make_unique<TaikoPlayResult>();
    auto &s = result->summary;
    s = std::move(summary);
    r.Expect("end");
    s.endReason = static_cast<PlayEndReason>(r.Count(static_cast<std::uint64_t>(PlayEndReason::Interrupted)));
    s.initialTime = RhythmTime{r.Integer()};
    s.endTime = RhythmTime{r.Integer()};
    s.unprocessedNotes = r.Size();
    r.Expect("judgements");
    for (auto &count : result->judgements)
        count = r.Size();
    result->okCount = r.Size();
    result->poorCount = r.Size();
    r.Expect("ticks");
    result->acceptedTicks = r.Size();
    result->missedTicks = r.Size();
    r.Expect("groups");
    for (auto &group : result->groups)
    {
        group.total = r.Size();
        group.processed = r.Size();
        group.accuracy = r.Accuracy();
        group.succeeded = r.Size();
    }
    r.Expect("timing");
    result->meanErrorMilliseconds = r.Optional();
    result->varianceMillisecondsSquared = r.Optional();
    result->standardDeviationMilliseconds = r.Optional();
    r.Expect("controls");
    s.controls.clear();
    for (auto count = r.Count(); count > 0; --count)
    {
        PlayControlEvent control;
        control.type = static_cast<PlayControlType>(r.Count(static_cast<std::uint64_t>(PlayControlType::Retire)));
        control.from = RhythmTime{r.Integer()};
        control.to = RhythmTime{r.Integer()};
        control.inputIndex = r.Size();
        control.pauseMenuShown = r.Flag();
        s.controls.push_back(control);
    }
    r.Expect("gauges");
    s.hpGauges.clear();
    for (auto count = r.Count(); count > 0; --count)
    {
        HpGaugeState gauge;
        gauge.percent = r.Number();
        gauge.applicable = r.Flag();
        gauge.passed = r.Flag();
        gauge.clearPercent = r.Number();
        s.hpGauges.push_back(gauge);
    }
    r.Expect("history");
    s.hpHistory.reset();
    if (const auto count = r.OptionalCount())
    {
        s.hpHistory.emplace();
        for (auto left = *count; left > 0; --left)
        {
            const RhythmTime time{r.Integer()};
            s.hpHistory->push_back({time, r.Number()});
        }
    }
    r.Expect("errors");
    for (auto count = r.Count(); count > 0; --count)
    {
        TimingErrorSample sample;
        sample.noteId = r.Count(std::numeric_limits<std::uint64_t>::max());
        sample.hitIndex = r.Size();
        sample.inputTime = RhythmTime{r.Integer()};
        sample.error = RhythmDuration{r.Integer()};
        sample.grade = static_cast<JudgementGrade>(r.Count(static_cast<std::uint64_t>(JudgementGrade::Unjudged)));
        sample.accuracy = r.Accuracy();
        result->timingErrors.push_back(sample);
    }
    r.End();
    return result;
}
