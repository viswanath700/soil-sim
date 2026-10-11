#include "soilsim/heightfield.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

namespace
{
void requireFinite(double value, const char* name)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}
} // namespace

namespace soilsim
{
Heightfield::Heightfield(std::size_t columns, std::size_t rows, double cellSize, double originX,
                         double originY, double initialHeight)
    : columns_(columns),
      rows_(rows),
      cellSize_(cellSize),
      originX_(originX),
      originY_(originY),
      heights_(columns * rows, initialHeight),
      moisture_(columns * rows, 0.0),
      frozenFraction_(columns * rows, 0.0),
      compaction_(columns * rows, 0.0)
{
    requireFinite(cellSize, "cellSize");
    requireFinite(originX, "originX");
    requireFinite(originY, "originY");
    requireFinite(initialHeight, "initialHeight");
    if (columns == 0 || rows == 0) {
        throw std::invalid_argument("heightfield dimensions must be positive");
    }
    if (cellSize <= 0.0) {
        throw std::invalid_argument("cellSize must be positive");
    }
}

std::size_t Heightfield::columns() const noexcept
{
    return columns_;
}

std::size_t Heightfield::rows() const noexcept
{
    return rows_;
}

double Heightfield::cellSize() const noexcept
{
    return cellSize_;
}

double Heightfield::originX() const noexcept
{
    return originX_;
}

double Heightfield::originY() const noexcept
{
    return originY_;
}

std::size_t Heightfield::index(std::size_t column, std::size_t row) const
{
    if (column >= columns_ || row >= rows_) {
        throw std::out_of_range("heightfield cell index is outside the grid");
    }
    return row * columns_ + column;
}

double& Heightfield::at(std::size_t column, std::size_t row)
{
    return heights_[index(column, row)];
}

const double& Heightfield::at(std::size_t column, std::size_t row) const
{
    return heights_[index(column, row)];
}

double Heightfield::sampleBilinear(double x, double y) const
{
    requireFinite(x, "x");
    requireFinite(y, "y");
    const double gridX = (x - originX_) / cellSize_ - 0.5;
    const double gridY = (y - originY_) / cellSize_ - 0.5;
    const double clampedX = std::clamp(gridX, 0.0, static_cast<double>(columns_ - 1));
    const double clampedY = std::clamp(gridY, 0.0, static_cast<double>(rows_ - 1));
    const auto x0 = static_cast<std::size_t>(std::floor(clampedX));
    const auto y0 = static_cast<std::size_t>(std::floor(clampedY));
    const auto x1 = std::min(x0 + 1, columns_ - 1);
    const auto y1 = std::min(y0 + 1, rows_ - 1);
    const double tx = clampedX - static_cast<double>(x0);
    const double ty = clampedY - static_cast<double>(y0);
    const double bottom = (1.0 - tx) * at(x0, y0) + tx * at(x1, y0);
    const double top = (1.0 - tx) * at(x0, y1) + tx * at(x1, y1);
    return (1.0 - ty) * bottom + ty * top;
}

std::pair<double, double> Heightfield::gradient(double x, double y) const
{
    requireFinite(x, "x");
    requireFinite(y, "y");
    const double halfCell = cellSize_ * 0.5;
    const double dx = (sampleBilinear(x + halfCell, y) - sampleBilinear(x - halfCell, y)) /
                      cellSize_;
    const double dy = (sampleBilinear(x, y + halfCell) - sampleBilinear(x, y - halfCell)) /
                      cellSize_;
    return {dx, dy};
}

double Heightfield::volumeAboveDatum(double datum) const
{
    requireFinite(datum, "datum");
    double volume = 0.0;
    for (const double height : heights_) {
        volume += std::max(0.0, height - datum) * cellSize_ * cellSize_;
    }
    return volume;
}

const std::vector<double>& Heightfield::heights() const noexcept
{
    return heights_;
}

std::vector<std::uint8_t> Heightfield::serializedBytes() const
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(2 * sizeof(std::uint64_t) + 3 * sizeof(double) +
                  heights_.size() * sizeof(double));
    const auto append = [&bytes](const auto& value) {
        const auto* begin = reinterpret_cast<const std::uint8_t*>(&value);
        bytes.insert(bytes.end(), begin, begin + sizeof(value));
    };
    const std::uint64_t columns = columns_;
    const std::uint64_t rows = rows_;
    append(columns);
    append(rows);
    append(cellSize_);
    append(originX_);
    append(originY_);
    for (const double height : heights_) {
        append(height);
    }
    return bytes;
}
} // namespace soilsim
