python
# ~/.gdbinit
# set auto-load safe-path /

import gdb.printing

class StringIdPrinter:
    def __init__(self, val):
        self.val = val
    def to_string(self):
        try:
            return self.val.call_method("to_cstring")
        except:
            return "ID: {}".format(self.val['_id'])
    def display_hint(self):
        return 'string'

def register_soot_printers():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("soot")
    pp.add_printer('StringId', '^carbon::StringId$', StringIdPrinter)
    gdb.printing.register_pretty_printer(None, pp, replace=True)

register_soot_printers()
end

# Пропуски стека
skip -gfi /usr/include/c++/*/*
skip -gfi /usr/include/c++/*
skip function std::*