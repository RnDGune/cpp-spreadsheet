#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <map>
#include <set>

class Sheet : public SheetInterface
{
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;


    void InvalidateCellCash(const Position& pos);
    // Добавляет зависемость
    void AddDependentCell(const Position& main_cell, const Position& dependent_cell);
    // Возвращяет зависемости
    const std::set<Position> GetDependentCells(const Position& pos);
    // Удаляет зависемости
    void DeleteDependencies(const Position& pos);

private:
    // Мапа всех зависемых ячеек листа, ячейка -> зависемые от неё
    std::map<Position, std::set<Position>> cells_dependencies_;
    /// Ячейки листа
    std::vector<std::vector<std::unique_ptr<Cell>>> sheet_;
   
    int max_row_ = 0;    // Число строк Printable Area
    int max_col_ = 0;    // Число столбцов Printable Area
    bool area_is_valid_ = true;    // валидность max_row_/col_

    void UpdatePrintableSize();
    bool CellExists(Position pos) const;
    // Резеврирует память для ячейки если её не существовало
    void Reserve(Position pos);
};