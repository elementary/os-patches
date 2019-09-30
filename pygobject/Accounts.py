from ..overrides import override
from ..importer import modules
from gi.repository import GObject

Accounts = modules['Accounts']._introspection_module

__all__ = []

def _get_string(self, key, default_value=None):
    value = GObject.Value()
    value.init(GObject.TYPE_STRING)
    if self.get_value(key, value) != Accounts.SettingSource.NONE:
        return value.get_string()
    else:
        return default_value

def _get_int(self, key, default_value=None):
    value = GObject.Value()
    value.init(GObject.TYPE_INT64)
    if self.get_value(key, value) != Accounts.SettingSource.NONE:
        return value.get_int64()
    else:
        return default_value

def _get_bool(self, key, default_value=None):
    value = GObject.Value()
    value.init(GObject.TYPE_BOOLEAN)
    if self.get_value(key, value) != Accounts.SettingSource.NONE:
        return value.get_boolean()
    else:
        return default_value

class Account(Accounts.Account):
    get_string = _get_string
    get_int = _get_int
    get_bool = _get_bool

    def get_settings_iter(self, prefix=''):
        return super(Account, self).get_settings_iter(prefix)

    def get_settings(self, prefix=''):
        itr = self.get_settings_iter(prefix)
        while True:
            success, key, value = itr.next()
            if success:
                yield (key, value)
            else:
                break

    def get_settings_dict(self, prefix=''):
        return dict(self.get_settings(prefix))

    def __eq__(self, other):
        return self.id == other.id
    def __ne__(self, other):
        return self.id != other.id

Account = override(Account)
__all__.append('Account')

class Service(Accounts.Service):
    def __eq__(self, other):
        return self.get_name() == other.get_name()
    def __ne__(self, other):
        return self.get_name() != other.get_name()

Service = override(Service)
__all__.append('Service')

class AccountService(Accounts.AccountService):
    get_string = _get_string
    get_int = _get_int
    get_bool = _get_bool

    def __eq__(self, other):
        return self.get_account() == other.get_account() and \
            self.get_service() == other.get_service()
    def __ne__(self, other):
        return self.get_account() != other.get_account() or \
            self.get_service() != other.get_service()

AccountService = override(AccountService)
__all__.append('AccountService')
