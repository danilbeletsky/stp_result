#pragma once

#include "domain/DomainError.h"

#include <exception>
#include <functional>
#include <iosfwd>
#include <string>

namespace pvz::cli {

// Ввод закончился (Ctrl+D / конец файла) — приложение завершается.
class InputClosed : public std::exception {
public:
    const char* what() const noexcept override { return "ввод закрыт"; }
};

// Пользователь ввёл пустую строку там, где ждали значение, — операция отменяется.
class OperationCancelled : public std::exception {
public:
    const char* what() const noexcept override { return "операция отменена"; }
};

// Чтение и проверка пользовательского ввода. Ничего не знает о предметной области,
// кроме того, что парсеры сообщают об ошибке через DomainError.
class Console {
public:
    Console(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

    std::ostream& out() { return out_; }

    // Строка без пробелов по краям. Бросает InputClosed в конце ввода.
    std::string ask(const std::string& prompt);

    // Повторяет вопрос, пока не будет введено целое число из [min, max].
    int askNumber(const std::string& prompt, int min, int max);

    // Да/нет: "д", "да", "y", "yes" / "н", "нет", "n", "no".
    bool confirm(const std::string& prompt);

    // Спрашивает значение и разбирает его парсером. Если парсер бросил DomainError,
    // показывает сообщение и спрашивает снова. Пустая строка — OperationCancelled.
    template <typename T>
    T askParsed(const std::string& prompt, const std::function<T(const std::string&)>& parse) {
        while (true) {
            const std::string line = ask(prompt);
            if (line.empty()) {
                throw OperationCancelled();
            }
            try {
                return parse(line);
            } catch (const DomainError& error) {
                printRetryHint(error.what());
            }
        }
    }

private:
    void printRetryHint(const std::string& message);

    std::istream& in_;
    std::ostream& out_;
};

} // namespace pvz::cli
