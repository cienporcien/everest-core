from pathlib import Path
from dataclasses import dataclass, asdict


from everest.core.util.block import BlockCalculator, BlockDefinition
from .common import get_module_manifest
from .interface import ILanguageGenerator, GeneratorConfig, CreateModuleOptions, FileCreationInfo


@dataclass
class EnumViewModel:
    pass


@dataclass
class TypeViewModel:
    pass


@dataclass
class ImplementationMetaViewModel:
    base_class_header: str
    interface: str
    desc: str
    type_headers: list[str]

    # tmpl_data['info']['hpp_guard'] = helpers.snake_case(mod).upper() + '_HPP'
    # mod_hpp_file = output_path / f'{mod}.hpp'
    # tmpl_data['info']['blocks'] = helpers.load_tmpl_blocks(mod_hpp_blocks, mod_hpp_file, update_flag)
    # mod_files['core'].append({
    #     'abbr': 'module.hpp',
    #     'path': mod_hpp_file,
    #     'content': templates['module.hpp'].render(tmpl_data),
    #     'last_mtime': mod_path.stat().st_mtime
    # })

    # get all the interfaces

    # (impl_hpp_file, impl_cpp_file) = get_implementation_file_paths(impl, impl_name)

    # output_path = mod_path.parent

    # # provided interface implementations (impl cpp & hpp)
    # for impl in tmpl_data['provides']:
    #     interface = impl['type']
    #     (impl_hpp_file, impl_cpp_file) = construct_impl_file_paths(impl)

    #     # load template data for interface
    #     if_def, last_mtime = load_interface_definition(interface)

    #     if_tmpl_data = generate_tmpl_data_for_if(interface, if_def, False)

    #     if_tmpl_data['info'].update({
    #         'hpp_guard': helpers.snake_case(f'{impl["id"]}_{interface}').upper() + '_IMPL_HPP',
    #         'config': impl['config'],
    #         'class_name': interface + 'Impl',
    #         'class_parent': interface + 'ImplBase',
    #         'module_header': f'../{mod}.hpp',
    #         'module_class': mod,
    #         'interface_implementation_id': impl['id']
    #     })

    #     if_tmpl_data['info']['blocks'] = helpers.load_tmpl_blocks(
    #         impl_hpp_blocks, output_path / impl_hpp_file, update_flag)

    #     # FIXME (aw): time stamp should include parent interfaces modification dates
    #     mod_files['interfaces'].append({
    #         'abbr': f'{impl["id"]}.hpp',
    #         'path': output_path / impl_hpp_file,
    #         'printable_name': impl_hpp_file,
    #         'content': templates['interface_impl.hpp'].render(if_tmpl_data),
    #         'last_mtime': last_mtime
    #     })

    #     mod_files['interfaces'].append({
    #         'abbr': f'{impl["id"]}.cpp',
    #         'path': output_path / impl_cpp_file,
    #         'printable_name': impl_cpp_file,
    #         'content': templates['interface_impl.cpp'].render(if_tmpl_data),
    #         'last_mtime': last_mtime
    #     })

    # cmakelists_file = output_path / 'CMakeLists.txt'
    # tmpl_data['info']['blocks'] = helpers.load_tmpl_blocks(cmakelists_blocks, cmakelists_file, update_flag)
    # mod_files['core'].append({
    #     'abbr': 'cmakelists',
    #     'path': cmakelists_file,
    #     'content': templates['cmakelists'].render(tmpl_data),
    #     'last_mtime': mod_path.stat().st_mtime
    # })

    # module.hpp
    # tmpl_data['info']['hpp_guard'] = helpers.snake_case(mod).upper() + '_HPP'
    # mod_hpp_file = output_path / f'{mod}.hpp'
    # tmpl_data['info']['blocks'] = helpers.load_tmpl_blocks(mod_hpp_blocks, mod_hpp_file, update_flag)
    # mod_files['core'].append({
    #     'abbr': 'module.hpp',
    #     'path': mod_hpp_file,
    #     'content': templates['module.hpp'].render(tmpl_data),
    #     'last_mtime': mod_path.stat().st_mtime
    # })

    # # module.cpp
    # mod_cpp_file = output_path / f'{mod}.cpp'
    # mod_files['core'].append({
    #     'abbr': 'module.cpp',
    #     'path': mod_cpp_file,
    #     'content': templates['module.cpp'].render(tmpl_data),
    #     'last_mtime': mod_path.stat().st_mtime
    # })

    # # doc.rst
    # mod_files['docs'].append({
    #     'abbr': 'doc.rst',
    #     'path': output_path / 'doc.rst',
    #     'content': templates['doc.rst'].render(tmpl_data),
    #     'last_mtime': mod_path.stat().st_mtime
    # })

    # # docs/index.rst
    # mod_files['docs'].append({
    #     'abbr': 'index.rst',
    #     'path': output_path / 'docs' / 'index.rst',
    #     'content': templates['index.rst'].render(tmpl_data),
    #     'last_mtime': mod_path.stat().st_mtime
    # })

    # for file_info in [*mod_files['core'], *mod_files['interfaces'], *mod_files['docs']]:
    #     file_info['printable_name'] = file_info['path'].relative_to(output_path)

    # return mod_files
