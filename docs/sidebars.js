module.exports = {
  guidesSidebar: [
    'intro',
    'getting-started',
    {
      type: 'category',
      label: 'Core Concepts',
      items: ['architecture', 'components'],
    },
    {
      type: 'category',
      label: 'Graphics',
      items: ['canvas', 'sprites', 'text-rendering'],
    },
    {
      type: 'category',
      label: 'Scenes',
      items: ['scene-management', 'scene-transitions'],
    },
  ],

  apiSidebar: [
    {
      type: 'category',
      label: 'Core',
      items: [
        'api/core/Object',
        'api/core/Component',
        'api/core/Scene',
        'api/core/SceneStateMachine',
      ],
    },
    {
      type: 'category',
      label: 'Graphics',
      items: [
        'api/graphics/Canvas',
        'api/graphics/Sprite',
        'api/graphics/TextRenderer',
      ],
    },
    {
      type: 'category',
      label: 'Utils',
      items: [
        'api/utils/DrawingHelpers',
        'api/utils/Noise',
        'api/utils/Polar',
      ],
    },
  ],
};
