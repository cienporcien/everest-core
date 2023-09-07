from pathlib import Path
from dataclasses import asdict

from everest.framework.model import Module

from everest.core.language_generator.interface import ILanguageGenerator, GeneratorConfig
from everest.core.language_generator.file_management import FileCreationInfo


from .renderer import Renderer, TemplateID
from .vm_factory import ViewModelFactory
from .util import get_implementation_file_paths, snake_case, clang_format
from .blocks import Blocks

blocks = Blocks()


class CppGenerator(ILanguageGenerator):
    def __init__(self, config: GeneratorConfig, disable_clang_format: bool = True, clang_format_base_path: Path = None):
        super().__init__(config)

        self._disable_clang_format = disable_clang_format
        self._clang_format_base_path = clang_format_base_path

        self._render = Renderer()
        self._vm_factory = ViewModelFactory()

    def update_module(self):
        pass

    def generate_interface(self):
        pass

    def generate_loader(self):
        pass

    def _clang_format(self, files: list[FileCreationInfo]):
        if self._disable_clang_format:
            return

        for file in files:
            file.content = clang_format(self._clang_format_base_path, file.path.suffix, file.content)

    def _generate_module_core_files(self, module: Module, module_path: Path):
        module_vm = self._vm_factory.create_module(module)
        hpp_file_path = module_path / f'{module.name}.hpp'

        last_mtime = (module_path / 'manifest.yaml').stat().st_mtime  # FIXME

        hpp_template_data = asdict(module_vm)
        hpp_template_data.get('info').update({
            'blocks': blocks.get_module_hpp_blocks(hpp_file_path, self._update_mode)
        })

        # FIXME (aw): we should split/separate hpp/cpp template data

        hpp_info = FileCreationInfo(
            abbreviation='module.hpp',
            printable_name=f'{module.name}.hpp',
            content=self._render(TemplateID.MODULE_HPP, hpp_template_data),
            path=hpp_file_path,
            last_mtime=last_mtime
        )

        cpp_info = FileCreationInfo(
            abbreviation='module.cpp',
            printable_name=f'{module.name}.cpp',
            content=self._render(TemplateID.MODULE_CPP, asdict(module_vm)),
            path=module_path / f'{module.name}.cpp',
            last_mtime=last_mtime
        )

        return [hpp_info, cpp_info]

    def _generate_module_implementation_files(self, module: Module, module_path: Path):
        impl_files: list[FileCreationInfo] = []
        for impl_name, impl in module.implements.items():
            interface_name = impl.interface

            interface, last_mtime = self._get_interface_model(interface_name)  # FIXME (aw): last_mtime!

            interface_vm = self._vm_factory.create_interface(interface)

            hpp_file_name, cpp_file_name = get_implementation_file_paths(impl.interface, impl_name)

            hpp_template_data = asdict(interface_vm)

            # FIXME (aw): how to get rid of this?
            hpp_template_data.get('info').update({
                'hpp_guard': snake_case(f'{impl_name}_{interface_name}').upper() + '_IMPL_HPP',
                # FIXME (aw): update flag!
                'blocks': blocks.get_implementation_hpp_blocks(module_path / hpp_file_name, self._update_mode),
                'config': [self._vm_factory.create_typed_item(name, e.type) for name, e in impl.config.items()],
                'class_name': interface_name + 'Impl',
                'class_parent': interface_name + 'ImplBase',
                'module_header': f'../{module.name}.hpp',
                'module_class': module.name,
                'interface_implementation_id': impl_name
            })

            impl_files.append(FileCreationInfo(
                abbreviation=f'{impl_name}.hpp',
                printable_name=hpp_file_name,
                content=self._render(TemplateID.INTF_IMPL_HPP, hpp_template_data),
                path=module_path / hpp_file_name,
                last_mtime=last_mtime
            ))

            impl_files.append(FileCreationInfo(
                abbreviation=f'{impl_name}.cpp',
                printable_name=hpp_file_name,
                content=self._render(TemplateID.INTF_IMPL_CPP, hpp_template_data),
                path=module_path / cpp_file_name,
                last_mtime=last_mtime
            ))

        return impl_files

    def _generate_module_files(self, module: Module, module_path: Path):
        core_files = self._generate_module_core_files(module, module_path)
        implementation_files = self._generate_module_implementation_files(module, module_path)

        self._clang_format(core_files + implementation_files)

        return {
            'core': core_files,
            'implementation': implementation_files
        }
