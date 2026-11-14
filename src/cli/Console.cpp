#include "cli/Console.h"

#include "domain/Text.h"

#include <charconv>
#include <istream>
#include <ostream>

namespace pvz::cli {

std::string Console::ask(const std::string& prompt) {
    out_ << prompt << std::flush;
    std::string line;
    if (!std::getline(in_, line)) {
        out_ << '\n';
        throw InputClosed();
    }
    return text::trim(line);
}

int Console::askNumber(const std::string& prompt, int min, int max) {
    while (true) {
        const std::string line = ask(prompt);
        int value = 0;
        const char* end = line.data() + line.size();
        const auto [ptr, ec] = std::from_chars(line.data(), end, value);
        if (!line.empty() && ec == std::errc() && ptr == end && value >= min && value <= max) {
            return value;
        }
        out_ << "  Введите число от " << min << " до " << max << ".\n";
    }
}

bool Console::confirm(const std::string& prompt) {
    while (true) {
        const std::string answer = text::toUpperAscii(ask(prompt + " (д/н): "));
        if (answer == "Y" || answer == "YES" || answer == "д" || answer == "да" ||
            answer == "Д" || answer == "ДА") {
            return true;
        }
        if (answer == "N" || answer == "NO" || answer == "н" || answer == "нет" ||
            answer == "Н" || answer == "НЕТ") {
            return false;
        }
        out_ << "  Ответьте «д» или «н».\n";
    }
}

void Console::printRetryHint(const std::string& message) {
    out_ << "  ✗ " << message << "\n    Попробуйте ещё раз или оставьте строку пустой для отмены.\n";
}

} // namespace pvz::cli
