import { defineConfig } from 'vitepress';

export default defineConfig({
  title: 'swaggercpp',
  description:
    'Modern C++23 library for reading, writing, validating, traversing and serving Swagger/OpenAPI documents.',
  lang: 'en-US',
  cleanUrls: true,
  lastUpdated: true,
  base: '/swagger-cpp/',

  head: [
    ['meta', { name: 'theme-color', content: '#85EA2D' }],
    ['meta', { property: 'og:title', content: 'swaggercpp' }],
    ['meta', { property: 'og:description', content: 'Professional C++23 OpenAPI toolkit.' }],
    ['meta', { property: 'og:type', content: 'website' }],
  ],

  themeConfig: {
    siteTitle: 'swaggercpp',

    nav: [
      { text: 'Guide', link: '/quick-start' },
      { text: 'API', link: '/api/' },
      { text: 'Examples', link: '/examples/' },
      { text: 'Packaging', link: '/packaging/' },
      {
        text: 'v0.2.1',
        items: [
          { text: 'Releases', link: 'https://github.com/stescobedo92/swagger-cpp/releases' },
          { text: 'Changelog', link: 'https://github.com/stescobedo92/swagger-cpp/releases' },
          { text: 'vcpkg port', link: 'https://github.com/microsoft/vcpkg/tree/master/ports/swaggercpp' },
        ],
      },
    ],

    sidebar: {
      '/': [
        {
          text: 'Getting started',
          items: [
            { text: 'Introduction', link: '/' },
            { text: 'Installation', link: '/install' },
            { text: 'Quick start', link: '/quick-start' },
            { text: 'Guide', link: '/guide' },
            { text: 'Architecture', link: '/architecture' },
          ],
        },
        {
          text: 'API reference',
          collapsed: false,
          items: [
            { text: 'Overview', link: '/api/' },
            { text: 'Document model', link: '/api/document' },
            { text: 'DocumentReader', link: '/api/reader' },
            { text: 'DocumentWriter', link: '/api/writer' },
            { text: 'DocumentValidator', link: '/api/validator' },
            { text: 'DocumentWalker', link: '/api/walker' },
            { text: 'SwaggerUiServer', link: '/api/ui' },
            { text: 'Result and Issues', link: '/api/result' },
          ],
        },
        {
          text: 'Examples',
          collapsed: false,
          items: [
            { text: 'Overview', link: '/examples/' },
            { text: 'Parse YAML', link: '/examples/parse-yaml' },
            { text: 'Parse JSON', link: '/examples/parse-json' },
            { text: 'Build from code', link: '/examples/build-from-code' },
            { text: 'Validate', link: '/examples/validate-document' },
            { text: 'Mutate', link: '/examples/mutate-document' },
            { text: 'Traverse', link: '/examples/traverse-document' },
            { text: 'Roundtrip', link: '/examples/roundtrip' },
            { text: 'Serve Swagger UI', link: '/examples/serve-ui' },
            { text: 'Endpoint catalog', link: '/examples/endpoint-catalog' },
          ],
        },
        {
          text: 'Packaging',
          collapsed: false,
          items: [
            { text: 'Overview', link: '/packaging/' },
            { text: 'vcpkg', link: '/packaging/vcpkg' },
            { text: 'Conan', link: '/packaging/conan' },
            { text: 'CMake package', link: '/packaging/cmake-package' },
            { text: 'FetchContent', link: '/packaging/fetchcontent' },
          ],
        },
      ],
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/stescobedo92/swagger-cpp' },
    ],

    search: {
      provider: 'local',
    },

    editLink: {
      pattern:
        'https://github.com/stescobedo92/swagger-cpp/edit/master/docs/:path',
      text: 'Edit this page on GitHub',
    },

    footer: {
      message: 'Released under the MIT License.',
      copyright: 'Copyright © 2026 Sergio Triana Escobedo',
    },

    outline: {
      level: [2, 3],
      label: 'On this page',
    },
  },
});
