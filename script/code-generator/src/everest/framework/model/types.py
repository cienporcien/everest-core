from dataclasses import dataclass
from typing import Union
import itertools
from graphlib import TopologicalSorter

"""
what we build here as a dataclass model, basically already exists as a dict
it's questionable if we really need to model this again 1:1 here
"""


@dataclass(frozen=True)
class TypeReference:
    ID = 'reference'
    name: str
    unit: str

    @property
    def namespaces(self) -> list[str]:
        return self.unit.split('/')

    @property
    def is_local(self):
        return self.unit == None


@dataclass
class NullType:
    ID = 'null'


@dataclass
class BooleanType:
    ID = 'boolean'


@dataclass
class IntegerType:
    ID = 'integer'
    format: str


@dataclass
class NumberType:
    ID = 'float'
    format: str


@dataclass
class StringType:
    ID = 'string'
    is_enum: bool


@dataclass
class ArrayType:
    ID = 'array'
    items: 'Type'


@dataclass
class VariantType:
    ID = 'variant'
    items: list['Type']


@dataclass
class JsonObjectProperty:
    name: str
    type: 'Type'


@dataclass
class ObjectType:
    ID = 'object'
    properties: list[JsonObjectProperty]


Type = Union[TypeReference, NullType, BooleanType, IntegerType, NumberType, StringType, ArrayType, VariantType, ObjectType]


@dataclass
class TypeUnit:
    name: str
    types: dict[str, Type]


def create_type_reference_from_url(ref: str):
    if not ref.startswith('/'):
        # this should be a local reference
        if not ref.startswith('#/'):
            raise Exception(f'Local type reference needs to start with #/: {ref}')

        return TypeReference(name=ref.lstrip('#/'), unit=None)

    if '#/' not in ref:
        raise Exception(f'Type reference needs to refer to a specific type with #/TypeName: {ref}')

    unit, name = ref.lstrip('/').split('#/')
    return TypeReference(name=name, unit=unit)


def create_object_from_definition(d: dict):
    # FIXME (aw): not handling required/additional properties ...
    properties = [JsonObjectProperty(name, create_type_from_definition(e))
                  for name, e in d.get('properties', {}).items()]
    return ObjectType(properties)


def create_variant_type_from_definition(types: list[str]):
    def create_simple_type(type_name: str):
        if type_name == 'null':
            return NullType()
        elif type_name == 'boolean':
            return BooleanType()
        elif type_name == 'integer':
            return IntegerType(None)
        elif type_name == 'number':
            return NumberType(None)
        elif type_name == 'string':
            return StringType(False)
        else:
            raise Exception(f'Variant type with does not yet support {type_name} type')

    return VariantType([create_simple_type(e) for e in types])


def create_type_from_definition(d: dict) -> Type:
    ref = d.get('$ref', None)

    if ref:
        return create_type_reference_from_url(ref)

    # so no ref, then type needs to be there
    type = d.get('type', None)

    if type == None:
        raise Exception('Type definition requires a type keyword, if no $ref is used')

    # dispatch the type
    if isinstance(type, list):
        return create_variant_type_from_definition(type)

    # FIXME (aw): do we need to check here, that type is indeed a string instance?
    if type == 'null':
        return NullType()
    elif type == 'boolean':
        return BooleanType()
    elif type == 'integer':
        return IntegerType(format=d.get('format', None))
    elif type == 'number':
        return NumberType(format=d.get('format', None))
    elif type == 'string':
        # FIXME (aw): enum handling!
        return StringType(False)
    elif type == 'array':
        return ArrayType(items=create_type_from_definition(d.get('items')))
    elif type == 'object':
        return create_object_from_definition(d)

    raise Exception(f'Unknown type: {type}')


def create_type_unit_from_definition(d: dict, unit_name: str):
    return TypeUnit(
        name=unit_name,
        types={name: create_type_from_definition(e) for name, e in d.get('types', {}).items()}
    )


def get_dependent_types(type: Type) -> set[TypeReference]:
    if isinstance(type, TypeReference):
        return {type}
    elif isinstance(type, ObjectType):
        return set(itertools.chain.from_iterable([get_dependent_types(e.type) for e in type.properties]))
    elif isinstance(type, ArrayType):
        return get_dependent_types(type.items)
    else:
        return set()


def get_topological_sorted_type_list_from_unit(unit: TypeUnit) -> list[tuple[str, Type]]:
    def filter_local_ref(types: set[TypeReference]) -> list[str]:
        # FIXME (aw): is local should be set here already!
        return [e.name for e in types if e.unit == unit.name]

    full_graph = {name: get_dependent_types(e) for name, e in unit.types.items()}

    local_graph = {name: filter_local_ref(e) for name, e in full_graph.items()}

    ts = TopologicalSorter(local_graph)
    ts.prepare()

    while ts.is_active():
        nodes = ts.get_ready()
        print(sorted(nodes))
        ts.done(*nodes)
