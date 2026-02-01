const apiSidebarConfig = require('./api-sidebar.js');

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

   apiSidebar: apiSidebarConfig.apiSidebar,
 };
