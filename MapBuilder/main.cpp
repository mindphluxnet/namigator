#include "parser.hpp"
#include "Output/Continent.hpp"
#include "Worker.hpp"

#include <boost/program_options.hpp>

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main(int argc, char *argv[])
{
    std::string dataPath;
#ifdef _DEBUG
    int adtX, adtY;
#else
    int jobs;
#endif

    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
        ("data,d", boost::program_options::value<std::string>(&dataPath)->default_value(".")->required(),   "data folder")
#ifdef _DEBUG
        ("adtX,x", boost::program_options::value<int>(&adtX)->required(),                                   "adt x")
        ("adtY,y", boost::program_options::value<int>(&adtY)->required(),                                   "adt y")
#else
        ("jobs,j", boost::program_options::value<int>(&jobs)->required(),                                   "build jobs")
#endif
        ("help,h",                                                                                          "display help message");

    boost::program_options::variables_map vm;

    try
    {
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
        boost::program_options::notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        }
    }
    catch (boost::program_options::error const &e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        std::cerr << desc << std::endl;

        return EXIT_FAILURE;
    }

    parser::Parser::Initialize();

#ifdef _DEBUG
    std::cout << "Debug mode." << std::endl;

    {
        parser::Continent azeroth("Azeroth");
        Worker worker(&azeroth);

        worker.EnqueueAdt(32, 48);

        std::cout << "Waiting for worker..." << std::endl;

        while (!!worker.Jobs())
        {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }

        std::cout << "Workers are not waiting for main thread.  Scope-destruction incoming..." << std::endl;
    }

    std::cout << "Finished." << std::endl;
#else
    std::cerr << "En masse mesh construction has not been implemented yet" << std::endl;
#endif

    return EXIT_SUCCESS;
}