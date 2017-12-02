#pragma once
#include <named_type/named_type.hpp>
#include <limits>
#include <nonstd/optional.hpp>
#include <plog/Record.h>

#include <QMetaType>

using LineOffset = fluent::NamedType<int64_t, struct line_offset,
                            fluent::Addable, fluent::Incrementable,
                            fluent::Subtractable,
                            fluent::Comparable, fluent::Printable>;

LineOffset operator"" _offset(unsigned long long int value);


using LineNumber = fluent::NamedType<uint32_t, struct line_number,
                            fluent::Addable, fluent::Incrementable,
                            fluent::Subtractable, fluent::Decrementable,
                            fluent::Comparable, fluent::Printable>;

LineNumber operator"" _number(unsigned long long int value);


using LinesCount = fluent::NamedType<uint32_t, struct lines_count,
                            fluent::Addable, fluent::Incrementable,
                            fluent::Subtractable, fluent::Decrementable,
                            fluent::Comparable, fluent::Printable>;

LinesCount operator"" _count(unsigned long long int value);


using LineLength = fluent::NamedType<int, struct lines_count,
                            fluent::Comparable, fluent::Printable>;

LineLength operator"" _length(unsigned long long int value);

template<typename StrongType>
StrongType maxValue()
{
    return StrongType(std::numeric_limits<typename StrongType::UnderlyingType>::max());
}

using OptionalLineNumber = nonstd::optional<LineNumber>;

template <typename T, typename Parameter, template<typename> class... Skills>
plog::util::nstringstream& operator<<(plog::util::nstringstream& os, fluent::NamedType<T, Parameter, Skills...> const& object)
{
    os << object.get();
    return os;
}

namespace plog {

inline Record& operator<<(Record& record, const OptionalLineNumber& t)
{
    if (t) {
        t->print(record);
    }
    else {
        record << "none";
    }

    return record;
}

}

// Represents a position in a file (line, column)
class FilePosition
{
  public:
    FilePosition() : column_{-1} {}
    FilePosition( LineNumber line, int column )
    { line_ = line; column_ = column; }

    LineNumber line() const { return line_; }
    int column() const { return column_; }

  private:
    LineNumber line_;
    int column_;
};

LineNumber operator+(const LineNumber& number, const LinesCount& count);
LineNumber operator-(const LineNumber& number, const LinesCount& count);

Q_DECLARE_METATYPE(LineLength);
Q_DECLARE_METATYPE(LineNumber);
Q_DECLARE_METATYPE(LinesCount);

// Use a bisection method to find the given line number
// in a sorted list.
// The T type must be a container containing elements that
// implement the lineNumber() member.
// Returns true if the lineNumber is found, false if not
// foundIndex is the index of the found number or the index
// of the closest greater element.
template <typename T> bool lookupLineNumber(
        const T& list, LineNumber lineNumber, int* foundIndex )
{
    int minIndex = 0;
    int maxIndex = list.size() - 1;
    // If the list is not empty
    if ( maxIndex - minIndex >= 0 ) {
        // First we test the ends
        if ( list[minIndex].lineNumber() == lineNumber ) {
            *foundIndex = minIndex;
            return true;
        }
        else if ( list[maxIndex].lineNumber() == lineNumber ) {
            *foundIndex = maxIndex;
            return true;
        }

        // Then we test the rest
        while ( (maxIndex - minIndex) > 1 ) {
            const int tryIndex = (minIndex + maxIndex) / 2;
            const auto currentMatchingNumber =
                list[tryIndex].lineNumber();
            if ( currentMatchingNumber > lineNumber )
                maxIndex = tryIndex;
            else if ( currentMatchingNumber < lineNumber )
                minIndex = tryIndex;
            else if ( currentMatchingNumber == lineNumber ) {
                *foundIndex = tryIndex;
                return true;
            }
        }

        // If we haven't found anything...
        // ... end of the list or before the next
        if ( lineNumber > list[maxIndex].lineNumber() )
            *foundIndex = maxIndex + 1;
        else if ( lineNumber > list[minIndex].lineNumber() )
            *foundIndex = minIndex + 1;
        else
            *foundIndex = minIndex;
    }
    else {
        *foundIndex = 0;
    }

    return false;
}

template<typename Iterator>
LineNumber lookupLineNumber( Iterator begin, Iterator end, LineNumber lineNum )
{
    Iterator lowerBound = std::lower_bound( begin, end, lineNum );
    return LineNumber(std::distance(begin, lowerBound));
}
