import gdb.printing
import gdb.types


class LitPrinter:
    "Print a Lit"

    def __init__(self, val):
        sign_f = gdb.lookup_global_symbol("Glucose::sign(Glucose::Lit)").value()
        var_f = gdb.lookup_global_symbol("Glucose::var(Glucose::Lit)").value()
        self.val = val
        self.sign = sign_f(self.val)
        self.var = int(var_f(self.val))

    def to_string(self):
        if str(self.sign) == 'true':
            # +1, so that inner and outer representations match
            return '~x%d' % (self.var + 1)
        else:
            return 'x%d' % (self.var + 1)
