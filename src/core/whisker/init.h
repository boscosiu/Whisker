#ifndef WHISKER_INIT_H
#define WHISKER_INIT_H

namespace Json {
class Value;
}

namespace whisker {

class Init final {
  public:
    class Context {
      public:
        Context(int* argc, char*** argv);
        virtual ~Context() = default;

        int Run();

      private:
        virtual void InitContext(Json::Value&& config) {}
    };

    static void InitLogging(int* argc, char*** argv);

    // Suspends the system's termination signal handler (SIGINT, SIGQUIT, SIGTERM, CTRL-C/CTRL-BREAK on Windows)
    // and installs a new one which unblocks the caller of WaitForExitSignal().
    static void EnableExitSignalHandler();

    static void WaitForExitSignal();
};

}  // namespace whisker

#endif  // WHISKER_INIT_H
