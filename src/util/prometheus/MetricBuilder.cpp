//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "util/prometheus/MetricBuilder.hpp"

#include "util/Assert.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"
#include "util/prometheus/Histogram.hpp"
#include "util/prometheus/MetricBase.hpp"

#include <concepts>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace util::prometheus {

std::unique_ptr<MetricBase>
MetricBuilder::operator()(
    std::string name,
    std::string labelsString,
    MetricType const type,
    std::vector<std::int64_t> const& buckets
)
{
    ASSERT(type != MetricType::HistogramDouble, "Wrong metric type. Probably wrong bucket type was used.");
    if (type == MetricType::HistogramInt) {
        return makeHistogram(std::move(name), std::move(labelsString), type, buckets);
    }
    ASSERT(buckets.empty(), "Buckets must be empty for non-histogram types.");
    return makeMetric(std::move(name), std::move(labelsString), type);
}

std::unique_ptr<MetricBase>
MetricBuilder::operator()(
    std::string name,
    std::string labelsString,
    MetricType const type,
    std::vector<double> const& buckets
)
{
    ASSERT(type == MetricType::HistogramDouble, "This method is for HISTOGRAM_DOUBLE only.");
    return makeHistogram(std::move(name), std::move(labelsString), type, buckets);
}

std::unique_ptr<MetricBase>
MetricBuilder::makeMetric(std::string name, std::string labelsString, MetricType const type)
{
    switch (type) {
        case MetricType::CounterInt:
            return std::make_unique<CounterInt>(name, labelsString);
        case MetricType::CounterDouble:
            return std::make_unique<CounterDouble>(name, labelsString);
        case MetricType::GaugeInt:
            return std::make_unique<GaugeInt>(name, labelsString);
        case MetricType::GaugeDouble:
            return std::make_unique<GaugeDouble>(name, labelsString);
        case MetricType::HistogramInt:
            [[fallthrough]];
        case MetricType::HistogramDouble:
            [[fallthrough]];
        case MetricType::Summary:
            [[fallthrough]];
        default:
            ASSERT(false, "Unknown metric type: {}", static_cast<int>(type));
    }
    return nullptr;
}

template <typename ValueType>
    requires std::same_as<ValueType, std::int64_t> || std::same_as<ValueType, double>
std::unique_ptr<MetricBase>
MetricBuilder::makeHistogram(
    std::string name,
    std::string labelsString,
    MetricType type,
    std::vector<ValueType> const& buckets
)
{
    switch (type) {
        case MetricType::HistogramInt: {
            if constexpr (std::same_as<ValueType, std::int64_t>) {
                return std::make_unique<HistogramInt>(std::move(name), std::move(labelsString), buckets);
            } else {
                ASSERT(false, "Wrong bucket type for HISTOGRAM_INT.)");
                break;
            }
        }
        case MetricType::HistogramDouble:
            if constexpr (std::same_as<ValueType, double>) {
                return std::make_unique<HistogramDouble>(std::move(name), std::move(labelsString), buckets);
            } else {
                ASSERT(false, "Wrong bucket type for HISTOGRAM_DOUBLE.");
                break;
            }
        case MetricType::CounterInt:
            [[fallthrough]];
        case MetricType::CounterDouble:
            [[fallthrough]];
        case MetricType::GaugeInt:
            [[fallthrough]];
        case MetricType::GaugeDouble:
            [[fallthrough]];
        case MetricType::Summary:
            [[fallthrough]];
        default:
            ASSERT(false, "Unknown metric type: {}", static_cast<int>(type));
    }
    return nullptr;
}

}  // namespace util::prometheus
