#include <client/app/app.hpp>

int main(int argc, char** argv) {
  // Parse arguments to determine if GUI should be used
  auto config = client::ParseArguments(argc, argv);

  // Android: extract embedded model resources (packaged in the APK) to an actual
  // filesystem location and rewrite config paths accordingly.
  //
  // This is necessary because OpenCV cannot load models directly from Qt
  // resources (":/...") and the device won't see host-side copied directories.
  if (!client::ResolveEmbeddedModelsIfNeeded(config)) {
    // Continue anyway - models might be available on the filesystem
  }

  const bool use_gui = !config.headless;

  client::App app(argc, argv, config, use_gui);
  return client::ToExitCode(app.Run());
}
