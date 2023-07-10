#ifndef COLORING_RESULT_H
#define COLORING_RESULT_H

struct ColoringResult
{
    lli *colors;
    lli chromaticNumber;
    lli *labels = nullptr;

    ColoringResult(lli *colors, lli chromaticNumber, lli *labels = nullptr) {
        this->colors = colors;
        this->chromaticNumber = chromaticNumber;
        this->labels = labels;
    }
};

#endif // COLORING_RESULT_H