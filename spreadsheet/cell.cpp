#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>
#include <cmath>   


// Реализуйте следующие методы
Cell::Cell(SheetInterface& sheet)
    : sheet_(sheet)
{}

Cell::~Cell()
{
    if (impl_)
    {   
        impl_.reset(nullptr);    
    }
}

void Cell::Set(const std::string& text)
{
    using namespace std::literals;

    // Обработку пустой ячейки
    if (text.empty())
    {
        impl_ = std::make_unique<EmptyImpl>();
        return;
    }

    // Обработка текстовой ячейки
    if (text[0] != FORMULA_SIGN || (text[0] == FORMULA_SIGN && text.size() == 1))
    {
        impl_ = std::make_unique<TextImpl>(text);
        return;
    }
    // Пробуем распарсить и создать формулу, первый символ "=" отбрасываем
    try
    {
        impl_ = std::make_unique<FormulaImpl>(sheet_, std::string{ text.begin() + 1, text.end() });
    }
    catch (...)
    {
        // Если в ячейку записывают синтаксически некорректную формулу
        // кидаем исключение 
        std::string fe_msg = "Formula parsing error"s;
        throw FormulaException(fe_msg);
    }
}

void Cell::Clear()
{
    impl_ = std::make_unique<EmptyImpl>();
}

Cell::Value Cell::GetValue() const
{
    return impl_->IGetValue();
}

std::string Cell::GetText() const
{
    return impl_->IGetText();
}

std::vector<Position> Cell::GetReferencedCells() const
{
    return impl_.get()->IGetReferencedCells();
}

bool Cell::IsCyclicDependent(const Cell* start_cell_ptr, const Position& end_pos) const
{
    // Для всех зависемых ячеек
    for (const auto& referenced_cell_pos : GetReferencedCells())
    {
        // Позиция совпадает с конечной?
        // (циклическая зависимость найдена)
        if (referenced_cell_pos == end_pos)
        {
            return true;
        }
        // Пытаемся получить указатель на очередную ячейку из списка зависимости текущей
        const Cell* ref_cell_ptr = dynamic_cast<const Cell*>(sheet_.GetCell(referenced_cell_pos));
        // Если ссылка на еще не существующию ячейку
        if (!ref_cell_ptr)
        {
            // создаем для неё EmptyImpl 
            sheet_.SetCell(referenced_cell_pos, "");
            ref_cell_ptr = dynamic_cast<const Cell*>(sheet_.GetCell(referenced_cell_pos));
        }

        // Указатели начальной и текущей ячеек совпали
        // (циклическая зависимость найдена)
        if (start_cell_ptr == ref_cell_ptr)
        {
            return true;
        }

        // Рекурсия для всех зависемых ячеек 
        if (ref_cell_ptr->IsCyclicDependent(start_cell_ptr, end_pos))
        {
            return true;
        }
    }

    // Циклических зависемостей нет
    return false;
}

void Cell::InvalidateCache()
{
    impl_->IInvalidateCache();
}
bool Cell::IsCacheValid() const
{
    return impl_->ICached();
}



// ------------------ EmptyImpl ---------------------------

CellType Cell::EmptyImpl::IGetType() const
{
    return CellType::EMPTY;
}
CellInterface::Value Cell::EmptyImpl::IGetValue() const
{
    return 0.0;
}
std::string Cell::EmptyImpl::IGetText() const
{
    using namespace std::literals;
    return ""s;
}

std::vector<Position> Cell::EmptyImpl::IGetReferencedCells() const
{
    return {};
}
void Cell::EmptyImpl::IInvalidateCache()
{
    return;
}
bool Cell::EmptyImpl::ICached() const
{
    return true;
}

// ------------------ TextImpl ---------------------------

Cell::TextImpl::TextImpl(std::string text)
    : cell_text_(std::move(text))
{
    // Проверяем наличие экранирующих символов
    if (cell_text_[0] == ESCAPE_SIGN)
    {
        escaped_ = true;
    }
}
CellType Cell::TextImpl::IGetType() const
{
    return CellType::TEXT;
}
CellInterface::Value Cell::TextImpl::IGetValue() const
{
    if (escaped_)
    {
        // Возвращаем без экранирующего символа
        return cell_text_.substr(1, cell_text_.size() - 1);
    }
    else
    {
        return cell_text_;
    }
}
std::string Cell::TextImpl::IGetText() const
{
    return cell_text_;
}
std::vector<Position> Cell::TextImpl::IGetReferencedCells() const
{
    return {};
}

void Cell::TextImpl::IInvalidateCache()
{
    return;
}
bool Cell::TextImpl::ICached() const
{
    return true;
}

// ------------------ FormulaImpl ---------------------------

Cell::FormulaImpl::FormulaImpl(SheetInterface& sheet, std::string formula)
    : sheet_(sheet), formula_(ParseFormula(formula))
{}

CellType Cell::FormulaImpl::IGetType() const
{
    return CellType::FORMULA;
}
CellInterface::Value Cell::FormulaImpl::IGetValue() const
{
    // Если кэш валидный, возвращаем кешированное значение
    if (!cached_value_)
    {
        // Вычисляем
        FormulaInterface::Value result = formula_->Evaluate(sheet_);
        if (std::holds_alternative<double>(result))
        {
            // Вычислилосб и конечной
            if (std::isfinite(std::get<double>(result)))
            {
                return std::get<double>(result);
            }
            // Ошибка #DIV/0!
            else
            {
                return FormulaError(FormulaError::Category::Div0);
            }
        }

        // Ошибка вычесления, возвращаем
        return std::get<FormulaError>(result);
    }

    // Возвращаем кэшированное
    return *cached_value_;
}
std::string Cell::FormulaImpl::IGetText() const
{
    return { FORMULA_SIGN + formula_->GetExpression() };
}
std::vector<Position> Cell::FormulaImpl::IGetReferencedCells() const
{
    return formula_.get()->GetReferencedCells();
}
void Cell::FormulaImpl::IInvalidateCache()
{
    cached_value_.reset();
}
bool Cell::FormulaImpl::ICached() const
{
    return cached_value_.has_value();
}