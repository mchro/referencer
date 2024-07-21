project('referencer', 'cpp',
  version: '1.3.0',
  license: 'GPLv2',
  default_options: [
    'cpp_std=c++11'
  ],
  meson_version: '>= 0.48.0',
)

referencer_name = meson.project_name().to_lower()
referencer_version = meson.project_version()

referencer_prefix = get_option('prefix')
referencer_bindir = join_paths(referencer_prefix, get_option('bindir'))
referencer_datadir = join_paths(referencer_prefix, get_option('datadir'))
referencer_includedir = join_paths(referencer_prefix, get_option('includedir'))
referencer_libdir = join_paths(referencer_prefix, get_option('libdir'))
referencer_libexecdir = join_paths(referencer_prefix, get_option('libexecdir'))
referencer_localedir = join_paths(referencer_prefix, get_option('localedir'))
referencer_mandir = join_paths(referencer_prefix, get_option('mandir'))


gnome = import('gnome')
gnome.compile_schemas(
  build_by_default: true,
  depend_files: files('apps.referencer.gschema.xml')
)
install_data('apps.referencer.gschema.xml',
   install_dir: get_option('datadir') / 'glib-2.0/schemas',
)
gnome.post_install(glib_compile_schemas: true)

config_h = configuration_data()
# package
config_h.set_quoted('PACKAGE_VERSION', referencer_version)
config_h.set_quoted('VERSION', referencer_version)

# i18n
config_h.set_quoted('GETTEXT_PACKAGE', referencer_name)
config_h.set('ENABLE_NLS', true)


#gtkmm2_dep = dependency('gtkmm-2.4')
gtkmm3_dep = dependency('gtkmm-3.0')
python2_dep = dependency('python2')
libxml_dep = dependency('libxml-2.0')
poppler_dep = dependency('poppler-glib')
boost_regex_dep = dependency('boost', modules: ['regex'])
bibutils_dep = dependency('bibutils')

deps = [
  #gtkmm2_dep,
  gtkmm3_dep,
  python2_dep,
  libxml_dep,
  poppler_dep,
  boost_regex_dep,
  bibutils_dep,
]

top_inc = include_directories('./src')

referencer_localedir = join_paths(referencer_prefix, get_option('localedir'))
cflags = [
  '-DGNOMELOCALEDIR="@0@"'.format(referencer_localedir),
  '-DPLUGINDIR="@0@"'.format(referencer_libdir),
  '-DDATADIR="@0@"'.format(referencer_datadir),
]
ldflags = []

configure_file(
  output: 'config.h',
  configuration: config_h,
)

sources = files(
  'src/main.cpp',
  'src/ArxivPlugin.cpp',
  'src/BibData.cpp',
  'src/BibUtils.cpp',
  'src/cpprossRefPlugin.cpp',
  'src/Document.cpp',
  'src/DocumentCellRenderer.cpp',
  'src/DocumentList.cpp',
  'src/DocumentProperties.cpp',
  'src/DocumentTypes.cpp',
  'src/DocumentView.cpp',
  'src/EntryMulticppompletion.cpp',
  'src/Library.cpp',
  'src/Linker.cpp',
  'src/PluginManager.cpp',
  'src/Preferences.cpp',
  'src/Progress.cpp',
  'src/PythonDocument.cpp',
  'src/PythonPlugin.cpp',
  'src/RefWindow.cpp',
  'src/TagList.cpp',
  'src/ThumbnailGenerator.cpp',
  'src/Transfer.cpp',
  'src/Utility.cpp',
  'src/ustring.cpp',
)


executable(
  'referencer',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  cpp_args: cflags,
  link_args: ldflags,
  install: true,
)
