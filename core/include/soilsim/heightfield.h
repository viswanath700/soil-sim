#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

namespace soilsim
{
class Heightfield
{
public:
    Heightfield(std::size_t columns, std::size_t rows, double cellSize, double originX = 0.0,
                double originY = 0.0, double initialHeight = 0.0);

    std::size_t columns() const noexcept;
    std::size_t rows() const noexcept;
    double cellSize() const noexcept;
    double originX() const noexcept;
    double originY() const noexcept;

    double& at(std::size_t column, std::size_t row);
    const double& at(std::size_t column, std::size_t row) const;
    double sampleBilinear(double x, double y) const;
    std::pair<double, double> gradient(double x, double y) const;
    double volumeAboveDatum(double datum = 0.0) const;

    const std::vector<double>& heights() const noexcept;
    std::vector<std::uint8_t> serializedBytes() const;
    void writeBinary(const std::filesystem::path& path) const;
    static Heightfield readBinary(const std::filesystem::path& path);

private:
    std::size_t index(std::size_t column, std::size_t row) const;
    std::size_t columns_;
    std::size_t rows_;
    double cellSize_;
    double originX_;
    double originY_;
    std::vector<double> heights_;

    // Reserved for the future environment model.
    std::vector<double> moisture_;
    std::vector<double> frozenFraction_;
    std::vector<double> compaction_;
};
} // namespace soilsim
