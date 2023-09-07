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
    pass


@dataclass
class BooleanType:
    pass


@dataclass
class IntegerType:
    format: str


@dataclass
class NumberType:
    format: str


@dataclass
class StringType:
    is_enum: bool


@dataclass
class ArrayType:
    items: 'Type'


@dataclass
class JsonObjectProperty:
    name: str
    type: 'Type'


@dataclass
class ObjectType:
    properties: list[JsonObjectProperty]


Type = Union[TypeReference, NullType, BooleanType, IntegerType, NumberType, StringType, ArrayType, ObjectType]


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


def create_type_from_definition(d: dict):
    ref = d.get('$ref', None)

    if ref:
        return create_type_reference_from_url(ref)

    # so no ref, then type needs to be there
    type = d.get('type', None)

    if type == None:
        raise Exception('Type definition requires a type keyword')

    # dispatch the type
    if isinstance(type, list):
        raise NotImplementedError('Variadic types not yet implemeted')

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
