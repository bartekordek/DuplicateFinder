#pragma once

#include "CUL/String/StringWrapper.hpp"
#include "CUL/GenericUtils/NonCopyable.hpp"
#include "CUL/STL_IMPORTS/STD_vector.hpp"
#include "CUL/STL_IMPORTS/STD_mutex.hpp"

using PathList = std::vector<CUL::StringWr>;

class DuplicateFinderBase
{
public:
    DuplicateFinderBase();
    void addPath( const CUL::StringWr& inPath );
    void removePath( const CUL::StringWr& inPath );
    void removeLast();

    const PathList getPaths() const;
    ~DuplicateFinderBase();

protected:
private:

    mutable std::mutex m_searchPathsMtx;
    PathList m_searchPaths;
public:
    CUL_NONCOPYABLE(DuplicateFinderBase);
};