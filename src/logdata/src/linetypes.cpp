#include "data/linetypes.h"

LineOffset operator"" _offset(unsigned long long int value) { return LineOffset(value); }
LineNumber operator"" _number(unsigned long long int value) { return LineNumber(value); }
LinesCount operator"" _count(unsigned long long int value) { return LinesCount(value); }
LineLength operator"" _length(unsigned long long int value) { return LineLength(value); }


LineNumber operator+(const LineNumber& number, const LinesCount& count)
{
    uint64_t line = number.get() + count.get();
    return line > maxValue<LineNumber>().get()
            ? maxValue<LineNumber>()
            : LineNumber( static_cast<LineNumber::UnderlyingType>(line) );
}

LineNumber operator-(const LineNumber& number, const LinesCount& count)
{
    int line = number.get() - count.get();
    return line >= 0 ? LineNumber(line) : LineNumber(0);
}
