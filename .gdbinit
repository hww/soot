# В файле .gdbinit
import gdb

class StringIdPrinter:
    """Printer for carbon::lib::StringId"""
    
    def __init__(self, val):
        self.val = val
        
    def to_string(self):
        try:
            # Вызываем to_string() если метод доступен
            return gdb.parse_and_eval('((carbon::lib::StringId){}).to_cstring()'.format(self.val.address))
        except:
            # fallback на отображение числового значения
            return str(self.val['_id'])
            
    def display_hint(self):
        return 'string'

def register_printers(objfile):
    objfile.pretty_printers.append(lambda val: StringIdPrinter(val) if str(val.type) == 'carbon::lib::StringId' else None)

gdb.current_objfile().pretty_printers.append(register_printers)