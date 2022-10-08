#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>

using namespace std::literals;

Sheet::~Sheet()
{}

void Sheet::SetCell(Position pos, std::string text)
{
    if (!pos.IsValid())
    {
        throw InvalidPositionException("Invalid position for SetCell()");
    }

    // Выделяем память под ячейку, проверка внутри
    Reserve(pos);
    auto cell = GetCell(pos);
    //Ячейка существует
    if (cell)
    {
        // Сохранимся на случай отката
        std::string old_text = cell->GetText();

        //Очищаем все старое
        InvalidateCellCash(pos);
        DeleteDependencies(pos);
        dynamic_cast<Cell*>(cell)->Clear();
        // Пытаемся записать новое
        dynamic_cast<Cell*>(cell)->Set(text);
        // Проверяем на циклические зависимости новое содержимое 
        if (dynamic_cast<Cell*>(cell)->IsCyclicDependent(dynamic_cast<Cell*>(cell), pos))
        {
            // Циклическая зависимость найдена, откатываемся к старому значени.
            dynamic_cast<Cell*>(cell)->Set(std::move(old_text));
            throw CircularDependencyException("Circular dependency detected!");
        }

        // Сохраняем зависимости
        for (const auto& ref_cell : dynamic_cast<Cell*>(cell)->GetReferencedCells())
        {
            AddDependentCell(ref_cell, pos);
        }
    }
    // Новая ячейка
    else
    {
        auto new_cell = std::make_unique<Cell>(*this);
        new_cell->Set(text);

        // Проверяем на циклические зависимости
        if (new_cell.get()->IsCyclicDependent(new_cell.get(), pos))
        {
            throw CircularDependencyException("Circular dependency detected!");
        }

        // Добавляем зависимости
        for (const auto& ref_cell : new_cell.get()->GetReferencedCells())
        {
            AddDependentCell(ref_cell, pos);
        }

        // Вставляем в sheet_ новую ячейку
        sheet_.at(pos.row).at(pos.col) = std::move(new_cell);
        UpdatePrintableSize();
    }
}

const CellInterface* Sheet::GetCell(Position pos) const
{
    if (!pos.IsValid())
    {
        throw InvalidPositionException("Invalid position for GetCell()");
    }

    // Если ячейка существует
    if (CellExists(pos))
    {    
        if (sheet_.at(pos.row).at(pos.col)) // указатель не nullptr
        {           
            return sheet_.at(pos.row).at(pos.col).get();
        }
    }

    // Для  несуществующих ячеек возвращаем nullptr
    return nullptr;
}

CellInterface* Sheet::GetCell(Position pos)
{
    if (!pos.IsValid())
    {
        throw InvalidPositionException("Invalid position for GetCell()");
    }
    if (CellExists(pos))
    {
        if (sheet_.at(pos.row).at(pos.col))
        {
            return sheet_.at(pos.row).at(pos.col).get();
        }
    }
    return nullptr;
}

void Sheet::ClearCell(Position pos)
{
    if (!pos.IsValid())
    {
        throw InvalidPositionException("Invalid position for ClearCell()");
    }
    // Если ячейка существует
    if (CellExists(pos))
    {
        sheet_.at(pos.row).at(pos.col).reset();       // Удаляет содержимое ячейки

        // Если ячейка на границе, нужно обновить Printable Area
        if ((pos.row + 1 == max_row_) || (pos.col + 1 == max_col_))
        {
            area_is_valid_ = false;
            UpdatePrintableSize();
        }
    }
}

Size Sheet::GetPrintableSize() const
{
    if (area_is_valid_)
    {
        return Size{ max_row_, max_col_ };
    }  
    throw InvalidPositionException("The size of printable area has not been updated");
}

void Sheet::PrintValues(std::ostream& output) const
{
    for (int x = 0; x < max_row_; ++x)
    {
        bool need_separator = false;
        for (int y = 0; y < max_col_; ++y)
        {
            if (need_separator)
            {
                output << '\t';
            }
            need_separator = true;

            if ((y < static_cast<int>(sheet_.at(x).size())) && sheet_.at(x).at(y))
            {
                auto value = sheet_.at(x).at(y)->GetValue();
                if (std::holds_alternative<std::string>(value))
                {
                    output << std::get<std::string>(value);
                }
                if (std::holds_alternative<double>(value))
                {
                    output << std::get<double>(value);
                }
                if (std::holds_alternative<FormulaError>(value))
                {
                    output << std::get<FormulaError>(value);
                }
            }
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream& output) const
{
    for (int x = 0; x < max_row_; ++x)
    {
        bool need_separator = false;
        for (int y = 0; y < max_col_; ++y)
        {
            if (need_separator)
            {
                output << '\t';
            }
            need_separator = true;
            if ((y < static_cast<int>(sheet_.at(x).size())) && sheet_.at(x).at(y))
            {
                output << sheet_.at(x).at(y)->GetText();
            }
        }
        output << '\n';
    }
}

void Sheet::InvalidateCellCash(const Position& pos)
{
    for (const auto& dependent_cell : GetDependentCells(pos))
    {
        auto cell = GetCell(dependent_cell);
        dynamic_cast<Cell*>(cell)->InvalidateCache();
        InvalidateCellCash(dependent_cell);
    }
}

void Sheet::AddDependentCell(const Position& main_cell, const Position& dependent_cell)
{
    cells_dependencies_[main_cell].insert(dependent_cell);
}

const std::set<Position> Sheet::GetDependentCells(const Position& pos)
{
    if (cells_dependencies_.count(pos) != 0)
    {
        return cells_dependencies_.at(pos);
    }
    return {};
}

void Sheet::DeleteDependencies(const Position& pos)
{
    cells_dependencies_.erase(pos);
}

void Sheet::UpdatePrintableSize()
{
    max_row_ = 0;
    max_col_ = 0;

    for (int x = 0; x < static_cast<int>(sheet_.size()); ++x)
    {
        for (int y = 0; y < static_cast<int>(sheet_.at(x).size()); ++y)
        {
            if (sheet_.at(x).at(y))
            {
                max_row_ = std::max(max_row_, x + 1);
                max_col_ = std::max(max_col_, y + 1);
            }
        }
    }

    area_is_valid_ = true;
}

bool Sheet::CellExists(Position pos) const
{
    return (pos.row < static_cast<int>(sheet_.size())) && (pos.col < static_cast<int>(sheet_.at(pos.row).size()));
}

void Sheet::Reserve(Position pos)
{
    if (!pos.IsValid())
    {
        return;
    }
    if (static_cast<int>(sheet_.size()) < (pos.row + 1))
    {
        
        sheet_.reserve(pos.row + 1);
        sheet_.resize(pos.row + 1);
    }
    if (static_cast<int>(sheet_.at(pos.row).size()) < (pos.col + 1))
    {
        sheet_.at(pos.row).reserve(pos.col + 1);
        sheet_.at(pos.row).resize(pos.col + 1);
    }
}

std::unique_ptr<SheetInterface> CreateSheet()
{
    return std::make_unique<Sheet>();
}