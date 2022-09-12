#include "init.h"
#include <filesystem>
#include <fstream>
#include <utility>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <json/json.h>

DEFINE_string(config, "config.json", "Whisker JSON configuration file");

namespace whisker {

Init::Context::Context(int* argc, char*** argv) {
    InitLogging(argc, argv);
    EnableExitSignalHandler();
}

int Init::Context::Run() {
    LOG(INFO) << "Starting '" << gflags::ProgramInvocationShortName() << "' with config file "
              << std::filesystem::absolute(FLAGS_config).lexically_normal();

    std::ifstream file(FLAGS_config);
    CHECK(file) << "Error opening config file";
    Json::Value config;
    file >> config;

    InitContext(std::move(config));

    WaitForExitSignal();
    return 0;
}

void Init::InitLogging(int* argc, char*** argv) {
    FLAGS_logtostderr = true;
    gflags::ParseCommandLineFlags(argc, argv, true);
    google::InitGoogleLogging(gflags::ProgramInvocationShortName());
}

}  // namespace whisker

#ifdef _WIN32

#include <future>
#include <windows.h>

namespace whisker {

namespace {

std::promise<void> signal_promise;

BOOL WINAPI CtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        signal_promise.set_value();
        return TRUE;
    }
    return FALSE;
}

}  // namespace

void Init::EnableExitSignalHandler() {
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
}

void Init::WaitForExitSignal() {
    signal_promise.get_future().wait();
    LOG(INFO) << "Received exit signal";
}

}  // namespace whisker

#else

#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace whisker {

namespace {

int signal_pipe[2];

void SignalHandler(int signal) {
    [[maybe_unused]] auto r = write(signal_pipe[1], "#", 1);
}

}  // namespace

void Init::EnableExitSignalHandler() {
    // the classic self-pipe trick -- signal handler writes to a pipe to unblock another thread
    CHECK_ERR(pipe(signal_pipe)) << "Error creating exit signal pipe";
    const auto flags = fcntl(signal_pipe[1], F_GETFL);
    fcntl(signal_pipe[1], F_SETFL, flags | O_NONBLOCK);

    struct sigaction sa = {};
    sa.sa_handler = SignalHandler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = (SA_RESETHAND | SA_RESTART);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

void Init::WaitForExitSignal() {
    // wait for signal handler to write to pipe by blocking on poll()
    pollfd poll_fd{};
    poll_fd.fd = signal_pipe[0];
    poll_fd.events = POLLIN;
    poll(&poll_fd, 1, -1);
    LOG(INFO) << "Received exit signal";
}

}  // namespace whisker

#endif
