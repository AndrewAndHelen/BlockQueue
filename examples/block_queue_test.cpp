#include "AsyncTaskExecutor.h"

#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

std::pair<std::future<void>, bool> RunAsyncTask(tf::Taskflow* tf);

namespace {
struct FileIOTask
{
    string fileName;
    string NonGroundImageSize;
    string GroundImageSize;
};

tf::Taskflow BuildWriteFileTask(const fs::path& path, const FileIOTask& task)
{
    tf::Taskflow taskflow;
    auto t = taskflow.emplace(
        [&]()
        {
            ofstream outfile(path);
            if (!outfile.is_open()) {
                return;
            }

            outfile << task.fileName << endl;
            outfile << task.NonGroundImageSize << endl;
            outfile << task.GroundImageSize << endl;
        }
    );

    return taskflow;
}

tf::Taskflow BuildReadFileTask(const fs::path& path, FileIOTask& task)
{
    tf::Taskflow taskflow;

    auto t = taskflow.emplace(
        [&]()
        {
            ifstream infile(path);
            if (!infile.is_open()) {
                return;
            }

            string line;
            stringstream ss;

            getline(infile, line);
            ss.str(line);
            ss >> task.fileName;

            getline(infile, line);
            ss.clear();
            ss.str(line);
            ss >> task.NonGroundImageSize;

            getline(infile, line);
            ss.clear();
            ss.str(line);
            ss >> task.GroundImageSize;
        }
    );

    return taskflow;
}

void TestAsyncExecutor1()
{
    FileIOTask writeTask, readTask;
    writeTask.fileName = "Hello world";
    writeTask.NonGroundImageSize = "1415";
    writeTask.GroundImageSize = "545";

    fs::path path = "./summary.txt";

    vector<tf::Taskflow> taskVec;
    taskVec.push_back(BuildWriteFileTask(path, writeTask));
    taskVec.push_back(BuildReadFileTask(path, readTask));

    vector<future<void>> futureVec;

    for_each(taskVec.begin(), taskVec.end(), [&futureVec](tf::Taskflow& task) {
        futureVec.push_back(GetAsyncExecutorWoker()->run(task)); });

    for_each(futureVec.begin(), futureVec.end(), [](std::future<void>& fu) {
        fu.wait(); });
}

tf::Taskflow BuildPrintTask(size_t id)
{
    tf::Taskflow taskflow;
    auto t = taskflow.emplace([=]()
        {
            std::this_thread::sleep_for(100ms);
            cout << "sleep finished\n";
        });

    return taskflow;
}

void TestAsyncExecutor2(size_t nums)
{
    vector<future<void>> futureVec;

    std::vector<tf::Taskflow> taskVec;
    for (size_t i = 0; i < nums; ++i) {
        taskVec.push_back(BuildPrintTask(i));
    }

    for_each(taskVec.begin(), taskVec.end(), [&futureVec](tf::Taskflow& task) {
        auto [fu, isOk] = RunAsyncTask(&task);
        cout << boolalpha << "True or False:" << isOk << endl;
        if (isOk) {
            futureVec.push_back(std::move(fu));
        }
        });

    for_each(futureVec.begin(), futureVec.end(), [](std::future<void>& fu) {
        fu.wait(); });
}

void TestAsyncExecutor3(size_t nums)
{
    vector<std::future<void>> futureVec;

    std::vector<tf::Taskflow> taskVec;
    for (size_t i = 0; i < nums; ++i) {
        taskVec.push_back(BuildPrintTask(i));
    }

    for_each(taskVec.begin(), taskVec.end(), [&futureVec](tf::Taskflow& task) {
        auto [fu, isOk] = RunAsyncTaskUntil(&task, 100ms);
        cout << boolalpha << "True or False:" << isOk << endl;
        if (isOk) {
            futureVec.push_back(std::move(fu));
        }
        });

    for_each(futureVec.begin(), futureVec.end(), [](std::future<void>& fu) {
        fu.wait(); });
}

void TestAsyncExecutor4(size_t nums)
{
    vector<std::future<void>> futureVec;

    std::vector<tf::Taskflow> taskVec;
    for (size_t i = 0; i < nums; ++i) {
        taskVec.push_back(BuildPrintTask(i));
    }

    for_each(taskVec.begin(), taskVec.end(), [&futureVec](tf::Taskflow& task) {
        auto fu = BlockRunAsyncTask(&task);
        futureVec.push_back(std::move(fu)); });

    for_each(futureVec.begin(), futureVec.end(), [](std::future<void>& fu) {
        fu.wait(); });
}
}


int main(int argc, char** argv)
{
	SetAsyncTaskQueMaxSize(10);

    TestAsyncExecutor1();
    TestAsyncExecutor2(20);
    TestAsyncExecutor3(20);
    TestAsyncExecutor4(20);
	return 0;
}