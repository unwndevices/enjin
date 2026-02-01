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
          sidebarPath: 'sidebars.js',
          editUrl: 'https://github.com/unwndevices/enjin/edit/main/docs/',
        },
        blog: false,
        theme: {
          customCss: require.resolve('./src/css/custom.css'),
        },
      },
    ],
  ],

  plugins: [],

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
            { label: 'Getting Started', to: '/docs/getting-started' },
            { label: 'API Reference', to: '/api' },
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
