#include "../../include/enjin2/core/scene.hpp"
#include "../../include/enjin2/components/drawable.hpp"
#include <algorithm>

namespace enjin2 {

// Template specializations for common pixel types
// Note: Pixel4 support requires drawable components to be templated
template void Scene::render<uint8_t>(ICanvas<uint8_t>& canvas);
template void Scene::renderObjects<uint8_t>(ICanvas<uint8_t>& canvas);

} // namespace enjin2