export module weweb:manifest;

import rstd;
import wescene.json;

using namespace rstd::prelude;
using rstd::path::Path;

export namespace weweb
{

struct WebManifest {
    String         title;
    String         entry_html;
    owe::Json      user_props;
    Option<String> preview;
    // general.supportsaudioprocessing: Wallpaper Engine calls the page's audio listeners only when it is true
    bool           supports_audio { false };
};

Option<WebManifest> LoadWebManifest(ref<Path> workshop_dir);

} // namespace weweb
