#include "taco/storage/file_io_mtx.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <climits>

#include "taco/tensor.h"
#include "taco/format.h"
#include "taco/error.h"
#include "taco/util/strings.h"
#include "taco/util/timers.h"
#include "taco/util/files.h"

using namespace std;

namespace taco {

TensorBase readMTX(std::string filename, const Format& format, bool pack) {
  std::fstream file;
  util::openStream(file, filename, fstream::in);
  TensorBase tensor = readMTX(file, format, pack);
  file.close();
  return tensor;
}

TensorBase readMTX(std::istream& stream, const Format& format, bool pack) {
  string line;
  if (!std::getline(stream, line)) {
    return TensorBase();
  }

  // Read Header
  std::stringstream lineStream(line);
  string head, type, formats, field, symmetry;
  lineStream >> head >> type >> formats >> field >> symmetry;
  taco_uassert(head=="%%MatrixMarket") << "Unknown header of MatrixMarket";
  // type = [matrix tensor]
  taco_uassert((type=="matrix") || (type=="tensor"))
                                       << "Unknown type of MatrixMarket";
  // formats = [coordinate array]
  // field = [real integer complex pattern]
  taco_uassert(field=="real")          << "MatrixMarket field not available";
  // symmetry = [general symmetric skew-symmetric Hermitian]
  taco_uassert((symmetry=="general") || (symmetry=="symmetric"))
                                       << "MatrixMarket symmetry not available";

  bool symm = (symmetry=="symmetric");

  TensorBase tensor;
  if (formats=="coordinate")
    tensor = readSparse(stream,format,symm);
  else if (formats=="array")
    tensor = readDense(stream,format,symm);
  else
    taco_uerror << "MatrixMarket format not available";

  if (pack) {
    tensor.pack();
  }

  return tensor;
}

TensorBase readSparse(std::istream& stream, const Format& format, bool symm) {
  string line;
  std::getline(stream,line);

  // Skip comments at the top of the file
  string token;
  do {
    std::stringstream lineStream(line);
    lineStream >> token;
    if (token[0] != '%') {
      break;
    }
  } while (std::getline(stream, line));

  // The first non-comment line is the header with dimensions
  vector<int> dimensions;
  char* linePtr = (char*)line.data();
  while (size_t dimension = strtoul(linePtr, &linePtr, 10)) {
    taco_uassert(dimension <= INT_MAX) << "Dimension exceeds INT_MAX";
    dimensions.push_back(static_cast<int>(dimension));
  }
  size_t nnz = dimensions[dimensions.size()-1];
  dimensions.pop_back();
  if (symm)
    taco_uassert(dimensions.size()==2) << "Symmetry only available for matrix";

  vector<int> coordinates;
  vector<double> values;
  coordinates.reserve(nnz*dimensions.size());
  values.reserve(nnz);

  while (std::getline(stream, line)) {
    linePtr = (char*)line.data();
    for (size_t i=0; i < dimensions.size(); i++) {
      long index = strtol(linePtr, &linePtr, 10);
      taco_uassert(index <= INT_MAX) << "Index exceeds INT_MAX";
      coordinates.push_back(static_cast<int>(index));
    }
    double val = strtod(linePtr, &linePtr);
    values.push_back(val);
  }

  // Create matrix
  TensorBase tensor(type<double>(), dimensions, format);
  if (symm)
    tensor.reserve(2*nnz);
  else
    tensor.reserve(nnz);

  // Insert coordinates
  std::vector<int> coord;
  for (size_t i = 0; i < nnz; i++) {
    coord.clear();
    for (size_t mode = 0; mode < dimensions.size(); mode++) {
      coord.push_back(coordinates[i*dimensions.size() + mode] -1);
    }
    tensor.insert(coord, values[i]);
    if (symm && coord.front() != coord.back()) {
      std::reverse(coord.begin(), coord.end());
      tensor.insert(coord, values[i]);
    }
  }

  return tensor;
}

TensorBase readDense(std::istream& stream, const Format& format, bool symm) {
  string line;
  std::getline(stream,line);

  // Skip comments at the top of the file
  string token;
  do {
    std::stringstream lineStream(line);
    lineStream >> token;
    if (token[0] != '%') {
      break;
    }
  } while (std::getline(stream, line));

  // The first non-comment line is the header with dimension sizes
  vector<int> dimensions;
  char* linePtr = (char*)line.data();
  while (size_t dimension = strtoul(linePtr, &linePtr, 10)) {
    taco_uassert(dimension <= INT_MAX) << "Dimension exceeds INT_MAX";
    dimensions.push_back(static_cast<int>(dimension));
  }
  if (symm)
    taco_uassert(dimensions.size()==2) << "Symmetry only available for matrix";

  vector<double> values;
  auto size = std::accumulate(begin(dimensions), end(dimensions),
                              1, std::multiplies<double>());
  values.reserve(size);

  while (std::getline(stream, line)) {
    linePtr = (char*)line.data();
    double val = strtod(linePtr, &linePtr);
    values.push_back(val);
  }

  // Create matrix
  TensorBase tensor(type<double>(), dimensions, format);
  if (symm)
    tensor.reserve(2*size);
  else
    tensor.reserve(size);

  // Insert coordinates
  std::vector<int> coord;
  for (auto n = 0; n<size; n++) {
    coord.clear();
    auto index=n;
    for (size_t mode = 0; mode < dimensions.size()-1; mode++) {
      coord.push_back(index%dimensions[mode]);
      index=index/dimensions[mode];
    }
    coord.push_back(index);
    tensor.insert(coord, values[n]);
    if (symm && coord.front() != coord.back()) {
      std::reverse(coord.begin(), coord.end());
      tensor.insert(coord, values[n]);
    }
  }

  return tensor;
}

void writeMTX(std::string filename, const TensorBase& tensor) {
  std::fstream file;
  util::openStream(file, filename, fstream::out);
  writeMTX(file, tensor);
  file.close();
}

void writeMTX(std::ostream& stream, const TensorBase& tensor) {
  if (isDense(tensor.getFormat()))
    writeDense(stream, tensor);
  else
    writeSparse(stream, tensor);
}

void writeSparse(std::ostream& stream, const TensorBase& tensor) {
  if(tensor.getOrder() == 2)
    stream << "%%MatrixMarket matrix coordinate real general" << std::endl;
  else
    stream << "%%MatrixMarket tensor coordinate real general" << std::endl;
  stream << "%"                                             << std::endl;
  stream << util::join(tensor.getDimensions(), " ") << " ";
  stream << tensor.getStorage().getIndex().getSize() << endl;
  for (auto& value : iterate<double>(tensor)) {
    for (int coord : value.first) {
      stream << coord+1 << " ";
    }
    stream << value.second << endl;
  }
}

void writeDense(std::ostream& stream, const TensorBase& tensor) {
  if(tensor.getOrder() == 2)
    stream << "%%MatrixMarket matrix array real general" << std::endl;
  else
    stream << "%%MatrixMarket tensor array real general" << std::endl;
  stream << "%"                                        << std::endl;
  stream << util::join(tensor.getDimensions(), " ") << " " << endl;
  for (auto& value : iterate<double>(tensor)) {
    stream << value.second << endl;
  }
}

}
