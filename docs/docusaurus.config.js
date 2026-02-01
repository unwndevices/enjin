const config = {
  title: 'enjin2 Documentation',
  tagline: 'Lightweight C++ game engine library',
  favicon: 'img/favicon.ico',

  url: 'https://unwndevices.github.io',
  baseUrl: '/enjin/',

  organizationName: 'unwndevices',
  projectName: 'enjin',

  onBrokenLinks: 'throw',
  onBrokenMarkdownLinks: 'warn',

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      {
        docs: {
          path: 'src',
          routeBasePath: '/',
          sidebarPath: 'sidebars.js',
          editUrl: 'https://github.com/unwndevices/enjin/edit/main/docs/',
          exclude: ['api/**'],
        },
        blog: false,
        theme: {
          customCss: require.resolve('./src/css/custom.css'),
        },
      },
    ],
   ],

  plugins: [
    [
      '@docusaurus/plugin-content-docs',
      {
        id: 'api',
        path: 'src/api',
        routeBasePath: 'api',
        sidebarPath: 'api-sidebar.js',
        showLastUpdateAuthor: false,
        showLastUpdateTime: false,
      },
    ],
  ],

  themeConfig: {
    image: 'img/docusaurus-social-card.jpg',

    navbar: {
      title: 'enjin2',
      logo: {
        alt: 'enjin2 Logo',
        src: 'img/logo.svg',
      },
      items: [
         {
          type: 'docSidebar',
          sidebarId: 'guidesSidebar',
          position: 'left',
          label: 'Guides',
        },
        {
          type: 'docSidebar',
          sidebarId: 'apiSidebar',
          position: 'left',
          label: 'API Reference',
        },
        {
          href: 'https://github.com/unwndevices/enjin',
          label: 'GitHub',
          position: 'right',
        },
      ],
    },

    footer: {
      style: 'dark',
      links: [
         {
          title: 'Documentation',
          items: [
            { label: 'Getting Started', to: '/getting-started' },
            { label: 'API Reference', to: '/api/core/Object' },
          ],
        },
        {
          title: 'Community',
          items: [
            { label: 'GitHub', href: 'https://github.com/unwndevices/enjin' },
          ],
        },
        {
          title: 'More',
          items: [
            { label: 'GitHub Issues', href: 'https://github.com/unwndevices/enjin/issues' },
          ],
        },
      ],
      copyright: `Copyright © ${new Date().getFullYear()} UNWN Devices. Built with Docusaurus.`,
    },

    prism: {
      additionalLanguages: ['cpp', 'cmake', 'bash'],
    },
  },
};

module.exports = config;
