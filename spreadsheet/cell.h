#pragma once

#include "common.h"
#include "formula.h"

#include <optional>
#include <functional>     
#include <unordered_set>  

// Тип ячейки
enum class CellType
{
    EMPTY,    // пустая, тип по дефолту
    TEXT,
    FORMULA,
    ERROR
};
//CellInterface в common.h
class Cell : public CellInterface {
public:
    Cell(SheetInterface& sheet);    
    ~Cell();

    void Set(const std::string& text);
    void Clear();

    CellInterface::Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    // Проверка циклической зависимости start_cell_ptr от end_pos
    bool IsCyclicDependent(const Cell* start_cell_ptr, const Position& end_pos) const;
    void InvalidateCache();
    bool IsCacheValid() const;

private:
    // Интерфейс ячейек
    class Impl
    {
    public:
        virtual ~Impl() = default;
        virtual CellType IGetType() const = 0;
        virtual CellInterface::Value IGetValue() const = 0;
        virtual std::string IGetText() const = 0;

        // Получить список ячеек, от которых зависит текущая
        virtual std::vector<Position> IGetReferencedCells() const = 0;    
        virtual void IInvalidateCache() = 0;     
        virtual bool ICached() const = 0;        
    };

    // Пустая ячейка
    class EmptyImpl : public Impl
    {
    public:
        EmptyImpl() = default;
        CellType IGetType() const override;
        // Возвратит пустую ячейку пустую строку
        CellInterface::Value IGetValue() const override;    
        // Возвратит пусту стрку
        std::string IGetText() const override;              

        std::vector<Position> IGetReferencedCells() const override; 
        void IInvalidateCache() override;
        bool ICached() const override;
    };

    // Ячейка с текстом
    class TextImpl : public Impl
    {
    public:
        explicit TextImpl(std::string text);
        CellType IGetType() const override;
        // Возвратит "человеческий" текст ячейки
        CellInterface::Value IGetValue() const override;   
        // Возвратит "машинный" текст 
        std::string IGetText() const override;              
        
        std::vector<Position> IGetReferencedCells() const override;
        void IInvalidateCache() override;
        bool ICached() const override;
    private:
        std::string cell_text_;
        bool escaped_ = false;    // Экранировано ли содержимое esc-символом 
    };

    // Ячейка с формулой
    class FormulaImpl : public Impl
    {
    public:
        FormulaImpl(SheetInterface& sheet_, std::string formula);
        CellType IGetType() const override;
        //Возвартит вычесленное значение
        CellInterface::Value IGetValue() const override;   
        // Возвратит текст формулы
        std::string IGetText() const override;             

        std::vector<Position> IGetReferencedCells() const override;
        void IInvalidateCache() override;
        bool ICached() const override;
    private:
        SheetInterface& sheet_;    
        std::unique_ptr<FormulaInterface> formula_;
        std::optional<CellInterface::Value> cached_value_;
    };
    // Класс-реализация ячейки
    std::unique_ptr<Impl> impl_;    
    // Лист таблицы, которому принадлежит ячейка
    SheetInterface& sheet_;        
};