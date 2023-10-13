import argparse
from pathlib import Path

from . import __version__

from .language_generator.cpp import CppGenerator
from .language_generator.interface import GeneratorMethod, GeneratorConfig, GenerateModuleOptions, GenerateTypeOptions, GenerateRuntimeOptions, ILanguageGenerator

from .language_generator import SupportedLanguage


def create_generator_config(args: argparse.Namespace) -> GeneratorConfig:
    working_directory = Path(args.work_dir)
    generated_output_dir = Path(args.output_dir) if 'output_dir' in args else working_directory / 'build/generated'

    return GeneratorConfig(
        working_directory=working_directory.resolve(),
        everest_tree=[Path(e).resolve() for e in args.everest_dir],
        schema_directory=Path(args.schemas_dir).resolve(),
        generated_output_directory=generated_output_dir.resolve()
    )


def create_language_generator(args: argparse.Namespace, config: GeneratorConfig) -> ILanguageGenerator:
    selected_language = args.lang

    if selected_language not in SupportedLanguage.__members__:
        raise Exception(f'unsupported language {selected_language}')

    if selected_language == SupportedLanguage.cpp.name:
        return CppGenerator(
            config,
            disable_clang_format=args.disable_clang_format,
            clang_format_base_path=args.clang_format_file
        )


def dispatch_arguments(args: argparse.Namespace):
    generator_config = create_generator_config(args)
    generator = create_language_generator(args, generator_config)

    selected_method = args.selected_method

    if selected_method == GeneratorMethod.create_module or selected_method == GeneratorMethod.update_module:
        file_filter = args.only.split(',') if args.only else None
        options = GenerateModuleOptions(
            overwrite_if_exists=args.force,
            update_only=(selected_method == GeneratorMethod.update_module),
            show_diff=args.diff,
            file_filter=file_filter)

        generator.generate_module(options, args.module)

    elif selected_method == GeneratorMethod.generate_type:
        options = GenerateTypeOptions(args.force, args.diff)
        generator.generate_type(options, args.types)

    elif selected_method == GeneratorMethod.generate_loader:
        generator.generate_loader(args.module)


def main():

    parser = argparse.ArgumentParser(description='Everest command line tool')
    parser.add_argument('--version', action='version', version=f'%(prog)s {__version__}')

    parser.add_argument("--work-dir", "-wd", type=str,
                        help='work directory containing the manifest definitions (default: .)', default=str(Path.cwd()))
    parser.add_argument("--everest-dir", "-ed", nargs='*',
                        help='everest directory containing the interface definitions (default: .)', default=[str(Path.cwd())])
    parser.add_argument("--schemas-dir", "-sd", type=str,
                        help='everest framework directory containing the schema definitions (default: ../everest-framework/schemas)',
                        default=str(Path.cwd() / '../everest-framework/schemas'))

    # this is cpp related
    parser.add_argument('-l', '--lang', default=SupportedLanguage.cpp.name, const=SupportedLanguage.cpp.name, nargs='?', choices=[
                        e.name for e in SupportedLanguage], help='set the language type, for which code should be generated (default: %(default)s)')

    lang_cpp_args = parser.add_argument_group('cpp specific')
    lang_cpp_args.add_argument("--clang-format-file", type=Path, default=Path.cwd(),
                               help='Path to the directory, containing the .clang-format file (default: .)')
    lang_cpp_args.add_argument("--disable-clang-format", action='store_true', default=False,
                               help="Set this flag to disable clang-format")

    subparsers = parser.add_subparsers(metavar='<command>', help='available commands', required=True)
    parser_mod = subparsers.add_parser('module', aliases=['mod'], help='module related actions')
    parser_if = subparsers.add_parser('interface', aliases=['if'], help='interface related actions')
    parser_types = subparsers.add_parser('types', aliases=['ty'], help='type related actions')

    mod_actions = parser_mod.add_subparsers(metavar='<action>', help='available actions', required=True)
    mod_create_parser = mod_actions.add_parser('create', aliases=['c'], help='create module(s)')
    mod_create_parser.add_argument('module', type=str, help='name of the module, that should be created')
    mod_create_parser.add_argument('-f', '--force', action='store_true', help='force overwriting - use with care!')
    mod_create_parser.add_argument('-d', '--diff', '--dry-run', action='store_true',
                                   help='show resulting diff on create or overwrite')
    mod_create_parser.add_argument('--only', type=str, default=None,
                                   help='Comma separated filter list of module files, that should be created.  '
                                   'For a list of available files use "--only which".')
    mod_create_parser.set_defaults(selected_method=GeneratorMethod.create_module)

    mod_update_parser = mod_actions.add_parser('update', aliases=['u'], help='update module(s)')
    mod_update_parser.add_argument('module', type=str, help='name of the module, that should be updated')
    mod_update_parser.add_argument('-f', '--force', action='store_true', help='force overwriting')
    mod_update_parser.add_argument('-d', '--diff', '--dry-run', action='store_true', help='show resulting diff')
    mod_update_parser.add_argument('--only', type=str, default=None,
                                   help='Comma separated filter list of module files, that should be updated.  '
                                   'For a list of available files use "--only which".')
    mod_update_parser.set_defaults(selected_method=GeneratorMethod.update_module)

    # FIXME (aw): this should be something like module runtime generated things, could be a loader or also other clutter
    mod_genld_parser = mod_actions.add_parser(
        'generate-loader', aliases=['gl'], help='generate everest loader')
    mod_genld_parser.add_argument(
        'module', type=str, help='name of the module, for which the loader should be generated')
    mod_genld_parser.add_argument('-o', '--output-dir', type=str, help='Output directory for generated loader '
                                  'files (default: {everest-dir}/build/generated/generated/modules)')
    mod_genld_parser.set_defaults(selected_method=GeneratorMethod.generate_loader)

    types_actions = parser_types.add_subparsers(metavar='<action>', help='available actions', required=True)
    types_genhdr_parser = types_actions.add_parser(
        'generate-headers', aliases=['gh'], help='generete type headers')
    types_genhdr_parser.add_argument('-f', '--force', action='store_true', help='force overwriting')
    types_genhdr_parser.add_argument('-o', '--output-dir', type=str, help='Output directory for generated type '
                                     'headers (default: {everest-dir}/build/generated/generated/types)')
    types_genhdr_parser.add_argument('-d', '--diff', '--dry-run', action='store_true', help='show resulting diff')
    types_genhdr_parser.add_argument('types', nargs='*', help='a list of types, for which header files should '
                                     'be generated - if no type is given, all will be processed and non-processable '
                                     'will be skipped')
    types_genhdr_parser.set_defaults(selected_method=GeneratorMethod.generate_type)

    if_actions = parser_if.add_subparsers(metavar='<action>', help='available actions', required=True)
    if_genhdr_parser = if_actions.add_parser(
        'generate-headers', aliases=['gh'], help='generate headers')
    if_genhdr_parser.add_argument('-f', '--force', action='store_true', help='force overwriting')
    if_genhdr_parser.add_argument('-o', '--output-dir', type=str, help='Output directory for generated interface '
                                  'headers (default: {everest-dir}/build/generated/generated/interfaces)')
    if_genhdr_parser.add_argument('-d', '--diff', '--dry-run', action='store_true', help='show resulting diff')
    if_genhdr_parser.add_argument('interfaces', nargs='*', help='a list of interfaces, for which header files should '
                                  'be generated - if no interface is given, all will be processed and non-processable '
                                  'will be skipped')
    if_genhdr_parser.set_defaults(selected_method=GeneratorMethod.generate_interface)

    args = parser.parse_args()

    dispatch_arguments(args)
