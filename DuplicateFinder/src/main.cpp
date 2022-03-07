#include "DuplicateFinder.hpp"

int main( int argc, char** argv )
{
    std::vector<std::string> arguments;
    for( size_t i = 0; i < argc; ++i )
    {
        arguments.push_back( std::string( argv[i] ) );
    }

    std::string searchPath = ".";
    if( argc > 1 )
    {
        searchPath = arguments[1];
    }

    DuplicateFinder df;
    df.search( searchPath, "Result.txt" );

    return 0;
}