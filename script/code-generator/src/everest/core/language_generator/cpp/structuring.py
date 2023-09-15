from dataclasses import dataclass

@dataclass
class IncludeBatch:
    name: str
    items: list[str]

IncludeBatches = list[IncludeBatch]
