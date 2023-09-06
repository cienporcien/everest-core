from pathlib import Path

from everest.core.util.block import BlockCalculator, BlockDefinition

class Blocks:
    def __init__(self):
        self._cpp_calculator = BlockCalculator(
            format='// ev@{uuid}:{version}',
            regex='^(?P<indent>\s*)// ev@(?P<uuid>[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}):(?P<version>.*)$'
        )
        self._cmake_calculator = BlockCalculator(
            format='// ev@{uuid}:{version}',
            regex='^(?P<indent>\s*)// ev@(?P<uuid>[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}):(?P<version>.*)$'
        )

    def get_cmakelist_blocks(self, file_path: Path, update: bool):
        return self._cmake_calculator.calculate({
            'add_general': BlockDefinition(
                id='bcc62523-e22b-41d7-ba2f-825b493a3c97',
                content='# insert your custom targets and additional config variables here'
            ),
            'add_other': BlockDefinition(
                id='c55432ab-152c-45a9-9d2e-7281d50c69c3',
                content='# insert other things like install cmds etc here'
            )
        }, 'v1', file_path, update)

    def get_implementation_hpp_blocks(self, file_path: Path, update: bool):
        return self._cpp_calculator.calculate({
            'add_headers': BlockDefinition(
                id='75ac1216-19eb-4182-a85c-820f1fc2c091',
                content='// insert your custom include headers here'
            ),
            'public_defs': BlockDefinition(
                id='8ea32d28-373f-4c90-ae5e-b4fcc74e2a61',
                content='// insert your public definitions here'
            ),
            'protected_defs': BlockDefinition(
                id='d2d1847a-7b88-41dd-ad07-92785f06f5c4',
                content='// insert your protected definitions here'
            ),
            'private_defs': BlockDefinition(
                id='3370e4dd-95f4-47a9-aaec-ea76f34a66c9',
                content='// insert your private definitions here'
            ),
            'after_class': BlockDefinition(
                id='3d7da0ad-02c2-493d-9920-0bbbd56b9876',
                content='// insert other definitions here'
            )
        }, 'v1', file_path, update)

    def get_module_hpp_blocks(self, file_path: Path, update: bool):
        return self._cpp_calculator.calculate({
            'add_headers': BlockDefinition(
                id='4bf81b14-a215-475c-a1d3-0a484ae48918',
                content='// insert your custom include headers here'
            ),
            'public_defs': BlockDefinition(
                id='1fce4c5e-0ab8-41bb-90f7-14277703d2ac',
                content='// insert your public definitions here'
            ),
            'protected_defs': BlockDefinition(
                id='4714b2ab-a24f-4b95-ab81-36439e1478de',
                content='// insert your protected definitions here'
            ),
            'private_defs': BlockDefinition(
                id='211cfdbe-f69a-4cd6-a4ec-f8aaa3d1b6c8',
                content='// insert your private definitions here'
            ),
            'after_class': BlockDefinition(
                id='087e516b-124c-48df-94fb-109508c7cda9',
                content='// insert other definitions here'
            )
        }, 'v1', file_path, update)
