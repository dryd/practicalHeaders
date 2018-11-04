#include <string>
#include <vector>
#include <map>
#include <iterator>
#include <fstream>
#include <algorithm>
#include <functional>

namespace pHcsv {

namespace detail {

static const std::istreambuf_iterator<char> EOCSVF;

inline std::string readCsvField(std::istreambuf_iterator<char>& it, bool& new_row) {
  std::string result;
  new_row = false;
  if (it == EOCSVF) {
    return result;
  }
  if (*it == '"') {
    it++;
    bool quote = false;
    while (it != EOCSVF) {
      char c = *it;
      it++;
      if (quote) {
        switch (c) {
          case '\n':
            new_row = true;
            return result;
          case ',':
            return result;
          case '"':
            result.push_back('"');
            quote = false;
            break;
          default:
            result.push_back('"');
            result.push_back(c);
            quote = false;
            break;
          }
        } else {
          switch (c) {
            case '"':
              quote = true;
              break;
            default:
              result.push_back(c);
              break;
          }
        }
    }
  } else {
    bool quote = false;
    while (it != EOCSVF) {
      char c = *it;
      it++;
      if (quote) {
        switch (c) {
          case '\n':
            new_row = true;
            return result;
          case ',':
            return result;
          case '"':
            quote = false;
            break;
          default:
            result.push_back(c);
            quote = false;
            break;
        }
      } else {
        switch (c) {
          case '\n':
            new_row = true;
            return result;
          case ',':
            return result;
          case '"':
            result.push_back('"');
            quote = true;
            break;
          default:
            result.push_back(c);
            break;
        }
      }
    }
  }
  return result;
}

inline std::vector<std::string> readCsvRow(std::istreambuf_iterator<char>& it, size_t reserve = 0) {
  std::vector<std::string> result;
  result.reserve(reserve);
  bool new_row = false;
  size_t i = 0;
  while (it != EOCSVF && !new_row) {
    result.push_back(readCsvField(it, new_row));
    i++;
  }
  for (; i < reserve; i++) {
    result.emplace_back();
  }
  return result;
}

inline std::map<std::string, std::string> readCsvRowToMap(std::istreambuf_iterator<char>& it, const std::vector<std::string>& header) {
  std::map<std::string, std::string> row_map;
  bool new_row = false;
  size_t h = 0;
  while (it != detail::EOCSVF && !new_row) {
    row_map.emplace(header.at(h), detail::readCsvField(it, new_row));
    h++;
  }
  for (; h < header.size(); h++) {
    row_map.emplace(header.at(h), "");
  }
  return row_map;
}

void writeCsvRow(std::ostreambuf_iterator<char>& it, const std::vector<std::string>& row) {
  for (size_t i = 0; i < row.size(); i++) {
    const auto& field = row.at(i);
    bool escape = false;
    for (char c : field) {
      if (c == ',' || c == '"') {
        escape = true;
        break;
      }
    }
    if (escape) {
      it = '"';
    }
    for (char c : field) {
      switch(c) {
        case '"':
          it = '"';
          it = '"';
          break;
        default:
          it = c;
          break;
      }
    }
    if (escape) {
      it = '"';
    }
    if (i != row.size() - 1) {
      it = ',';
    }
  }
}

template<typename T>
inline T convert(const std::string& str) {
  return T(str);  // hope it's constructible with a string...
}

template <>
inline std::string convert(const std::string& str) {
  return str;
}

template <>
inline double convert(const std::string& str) {
  return std::stod(str);
}

template <>
inline float convert(const std::string& str) {
  return std::stof(str);
}

template <>
inline int convert(const std::string& str) {
  return std::stoi(str);
}

template <>
inline size_t convert(const std::string& str) {
  return static_cast<size_t>(std::stoul(str));
}

}  // namespace detail

class dynamic {
 public:
  dynamic(std::istream& in, bool has_header) : header_(), data_() {
    readStream(in, has_header);
  }

  dynamic(const std::string& filename, bool has_header) : header_(), data_() {
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    readStream(in, has_header);
  }

  void write(std::ostream& out, bool with_header) const {
    writeStream(out, with_header);
  }

  void write(const std::string& filename, bool with_header) const {
    std::ofstream out(filename, std::ios::out | std::ios::binary);
    writeStream(out, with_header);
  }

  inline size_t headerIndex(const std::string& column) const {
    for (size_t i = 0; i < header_.size(); i++) {
      if (header_.at(i) == column) {
        return i;
      }
    }
    throw std::runtime_error("Unrecognized column " + column);
  }

  inline size_t size() const {
    return data_.size();
  }

  inline std::string& at(size_t row, const std::string& column) {
    rangeCheck(row);
    return data_[row][headerIndex(column)];
  }

  inline const std::string& at(size_t row, const std::string& column) const {
    rangeCheck(row);
    return data_[row][headerIndex(column)];
  }

  inline std::string& at(size_t row, size_t column) {
    rangeCheck(row, column);
    return data_[row][column];
  }

  inline const std::string& at(size_t row, size_t column) const {
    rangeCheck(row, column);
    return data_[row][column];
  }

  inline void addRow() {
    data_.emplace_back(header_.size(), "");
  }

  inline void addHeader(const std::string& column) {
    if (std::find(header_.begin(), header_.end(), column) == header_.end()) {
      header_.push_back(column);
      for (auto& row : data_) {
        row.emplace_back();
      }
    }
  }

  template <typename T = std::string>
  inline T get(size_t row, const std::string& column) const {
    return detail::convert<T>(at(row, column));
  }

  template <typename T = std::string>
  inline T get(size_t row, size_t column) const {
    return detail::convert<T>(at(row, column));
  }

  bool operator==(const dynamic& other) const {
    return header_ == other.header_ && data_ == other.data_;
  }

  bool operator!=(const dynamic& other) const {
    return !(*this == other);
  }

 private:
  void readStream(std::istream& in, bool has_header) {
    if (in.bad() || in.fail()) {
      throw std::runtime_error("Bad input");
    }
    std::istreambuf_iterator<char> it(in);
    if (has_header && it != detail::EOCSVF) {
      header_ = detail::readCsvRow(it);
    }
    while (it != detail::EOCSVF) {
      std::vector<std::string> row = detail::readCsvRow(it, header_.size());
      data_.push_back(std::move(row));
    }
  }

  void writeStream(std::ostream& out, bool with_header) const {
    if (out.bad() || out.fail()) {
      throw std::runtime_error("Bad output");
    }
    std::ostreambuf_iterator<char> it(out);
    if (with_header) {
      detail::writeCsvRow(it, header_);
      it = '\n';
    }
    for (size_t i = 0; i < data_.size(); i++) {
      detail::writeCsvRow(it, data_.at(i));
      if (i != data_.size() - 1) {
        it = '\n';
      }
    }
  }

  inline void rangeCheck(size_t row) const {
    if (row >= size()) {
      throw std::runtime_error("Row " + std::to_string(row) + " out of bounds");
    }
  }

  inline void rangeCheck(size_t row, size_t col) const {
    rangeCheck(row);
    if (col >= data_[row].size()) {
      throw std::runtime_error("Column " + std::to_string(col) + " out of bounds");
    }
  }

  std::vector<std::string> header_;
  std::vector<std::vector<std::string>> data_;
};

template <typename T>
class typed : public std::vector<T> {
 public:
  typed(const std::string& filename, std::function<T(const std::map<std::string, std::string>&)> parse_func) {
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    readStreamWithHeader(in, parse_func);
  }

  typed(std::ifstream& in, std::function<T(const std::map<std::string, std::string>&)> parse_func) {
    readStreamWithHeader(in, parse_func);
  }

  typed(const std::string& filename, std::function<T(const std::vector<std::string>&)> parse_func) {
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    readStreamNoHeader(in, parse_func);
  }

  typed(std::ifstream& in, std::function<T(const std::vector<std::string>&)> parse_func) {
    readStreamNoHeader(in, parse_func);
  }

  void write(const std::string& filename, std::function<std::map<std::string, std::string>(const T&)> map_func) const {
    std::ofstream out(filename, std::ios::out | std::ios::binary);
    writeStreamWithHeader(out, map_func);
  }

  void write(std::ofstream& out, std::function<std::map<std::string, std::string>(const T&)> map_func) const {
    writeStreamWithHeader(out, map_func);
  }

  void write(const std::string& filename, std::function<std::vector<std::string>(const T&)> vector_func) const {
    std::ofstream out(filename, std::ios::out | std::ios::binary);
    writeStreamNoHeader(out, vector_func);
  }

  void write(std::ofstream& out, std::function<std::vector<std::string>(const T&)> vector_func) const {
    writeStreamNoHeader(out, vector_func);
  }

  inline const std::vector<std::string>& header() const {
    return header_;
  }

  inline std::vector<std::string>& header() {
    return header_;
  }

 private:
  void readStreamWithHeader(std::istream& in, std::function<T(const std::map<std::string, std::string>&)> parse_func) {
    if (in.bad() || in.fail()) {
      throw std::runtime_error("Bad input");
    }
    std::istreambuf_iterator<char> it(in);
    if (it == detail::EOCSVF) {
      return;
    }
    header_ = detail::readCsvRow(it);
    while (it != detail::EOCSVF) {
      std::vector<T>::push_back(parse_func(detail::readCsvRowToMap(it, header_)));
    }
  }

  void readStreamNoHeader(std::istream& in, std::function<T(const std::vector<std::string>&)> parse_func) {
    if (in.bad() || in.fail()) {
      throw std::runtime_error("Bad input");
    }
    std::istreambuf_iterator<char> it(in);
    while (it != detail::EOCSVF) {
      std::vector<T>::push_back(parse_func(detail::readCsvRow(it)));
    }
  }

  void writeStreamWithHeader(std::ofstream& out, std::function<std::map<std::string, std::string>(const T&)> map_func) const {
    if (out.bad() || out.fail()) {
      throw std::runtime_error("Bad output");
    }
    std::ostreambuf_iterator<char> it(out);
    detail::writeCsvRow(it, header_);
    it = '\n';
    for (size_t i = 0; i < std::vector<T>::size(); i++) {
      std::map<std::string, std::string> data_map = map_func(std::vector<T>::at(i));
      std::vector<std::string> row(header_.size(), "");
      for (size_t h = 0; h < header_.size(); h++) {
        auto it = data_map.find(header_.at(h));
        if (it != data_map.end()) {
          row.at(h) = it->second;
        }
      }
      detail::writeCsvRow(it, row);
      if (i != std::vector<T>::size() - 1) {
        it = '\n';
      }
    }
  }

  void writeStreamNoHeader(std::ofstream& out, std::function<std::vector<std::string>(const T&)> vector_func) const {
    if (out.bad() || out.fail()) {
      throw std::runtime_error("Bad output");
    }
    std::ostreambuf_iterator<char> it(out);
    for (size_t i = 0; i < std::vector<T>::size(); i++) {
      detail::writeCsvRow(it, vector_func(std::vector<T>::at(i)));
      if (i != std::vector<T>::size() - 1) {
        it = '\n';
      }
    }
  }

  std::vector<std::string> header_;
};

}  // namespace pHcsv
