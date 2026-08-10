#pragma once
#include <Windows.h>
#include <memory>

namespace nl {

class App {
  public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    [[nodiscard]] bool Init(HINSTANCE inst);
    void Run();
    void Shutdown();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
