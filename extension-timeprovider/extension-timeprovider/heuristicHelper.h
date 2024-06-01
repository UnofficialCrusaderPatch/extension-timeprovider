#pragma once

#include <numeric>
#include <algorithm>
#include <vector>

template<typename T, size_t _Capacity, bool _RestrictToCapacity = true>
class HeuristicHelper
{
private:
  int currentArrayIndex;
  std::vector<T> values;

public:

  HeuristicHelper()
  {
    clear();
  }

  void clear()
  {
    values.clear();
    values.reserve(_Capacity);

    if constexpr (_RestrictToCapacity)
    {
      currentArrayIndex = 0;
    }
  }

  void pushValue(const int value)
  {
    if constexpr (!_RestrictToCapacity)
    {
      values.push_back(value);
      return;
    }

    if (values.size() < _Capacity)
    {
      values.push_back(value);
    }
    else
    {
      values[currentArrayIndex] = value;
    }
    ++currentArrayIndex;
    if (currentArrayIndex >= _Capacity)
    {
      currentArrayIndex = 0;
    }
  }

  T getAverage()
  {
    if (values.empty())
    {
      return static_cast<T>(0);
    }
    return std::accumulate(values.begin(), values.end(), static_cast<T>(0)) / values.size();
  }

  // source: https://stackoverflow.com/a/39487448
  T getMedian()
  {
    if (values.empty())
    {
      return static_cast<T>(0);
    }

    std::vector<T> tmpVector{ values };
    const size_t n = tmpVector.size() / 2;
    std::nth_element(tmpVector.begin(), tmpVector.begin() + n, tmpVector.end());
    if (tmpVector.size() % 2)
    {
      return tmpVector[n];
    }
    else
    {
      // even sized vector -> average the two middle values
      auto maxIt{ std::max_element(tmpVector.begin(), tmpVector.begin() + n) };
      return (*maxIt + tmpVector[n]) / 2;
    }
  }

  T getMin()
  {
    const auto res{ std::min(values.begin(), values.end()) };
    return res != values.end() ? *res : 0;
  }

  T getMax()
  {
    const auto res{ std::max(values.begin(), values.end()) };
    return res != values.end() ? *res : 0;
  }
};
