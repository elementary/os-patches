Python APT C++ API
==================
The C++ API provides functions to create Python objects from C++ objects and
to retrieve the C++ object stored in the Python object. An object may have
another Python object as its owner and keeps its owner alive for its
lifetime. Some objects require an owner of a specific type, while others
require none. Refer to the sections below for details.

The C++ API names use the name of the class in apt_pkg and are prefixed with
Py. For each supported class, there is a _Type object, a _Check() function,
a _CheckExact() function, a _FromCpp() and a _ToCpp() function.

.. versionadded:: 0.7.100

Acquire (pkgAcquire)
--------------------
.. c:var:: PyTypeObject PyAcquire_Type

    The type object for :class:`apt_pkg.Acquire` objects.

.. c:function:: int PyAcquire_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Acquire` object, or
    a subclass thereof.

.. c:function:: int PyAcquire_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Acquire` object and no
    subclass thereof.

.. c:function:: PyObject* PyAcquire_FromCpp(pkgAcquire *acquire, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.Acquire` object from the :c:type:`pkgAcquire`
    pointer given by the parameter *acquire*. If the parameter *delete* is
    true, the object pointed to by *acquire* will be deleted when the refcount
    of the return value reaches 0.

.. c:function:: pkgAcquire* PyAcquire_ToCpp(PyObject *acquire)

    Return the :c:type:`pkgAcquire` pointer contained in the Python object
    *acquire*.


AcquireFile (pkgAcqFile)
------------------------
.. c:var:: PyTypeObject PyAcquireFile_Type

    The type object for :class:`apt_pkg.AcquireFile` objects.

.. c:function:: int PyAcquireFile_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.AcquireFile` object, or
    a subclass thereof.

.. c:function:: int PyAcquireFile_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.AcquireFile` object
    and no subclass thereof.

.. c:function:: PyObject* PyAcquireFile_FromCpp(pkgAcqFile *file, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.AcquireFile` object from the :c:type:`pkgAcqFile`
    pointer given by the parameter *file*. If the parameter *delete* is
    true, the object pointed to by *file* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* should point
    to a :class:`apt_pkg.Acquire` object.

.. c:function:: pkgAcqFile* PyAcquireFile_ToCpp(PyObject *acquire)

    Return the :c:type:`pkgAcqFile` pointer contained in the Python object
    *acquire*.

AcquireItem (pkgAcquire::Item)
------------------------------
.. c:var:: PyTypeObject PyAcquireItem_Type

    The type object for :class:`apt_pkg.AcquireItem` objects.

.. c:function:: int PyAcquireItem_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.AcquireItem` object, or
    a subclass thereof.

.. c:function:: int PyAcquireItem_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.AcquireItem` object
    and no subclass thereof.

.. c:function:: PyObject* PyAcquireItem_FromCpp(pkgAcquire::Item *item, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.AcquireItem` object from the :c:type:`pkgAcquire::Item`
    pointer given by the parameter *item*. If the parameter *delete* is
    true, the object pointed to by *item* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* should point
    to a :class:`apt_pkg.Acquire` object.

.. c:function:: pkgAcquire::Item* PyAcquireItem_ToCpp(PyObject *object)

    Return the :c:type:`pkgAcquire::Item` pointer contained in the Python object
    *object*.

AcquireItemDesc (pkgAcquire::ItemDesc)
--------------------------------------
.. c:var:: PyTypeObject PyAcquireItemDesc_Type

    The type object for :class:`apt_pkg.AcquireItemDesc` objects.

.. c:function:: int PyAcquireItemDesc_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.AcquireItemDesc` object, or
    a subclass thereof.

.. c:function:: int PyAcquireItemDesc_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.AcquireItemDesc` object
    and no subclass thereof.

.. c:function:: PyObject* PyAcquireItemDesc_FromCpp(pkgAcquire::ItemDesc *desc, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.AcquireItemDesc` object from the :c:type:`pkgAcquire::ItemDesc`
    pointer given by the parameter *desc*. If the parameter *delete* is
    true, the object pointed to by *desc* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* should point
    to a :class:`apt_pkg.AcquireItem` object.

.. c:function:: pkgAcquire::ItemDesc* PyAcquireItemDesc_ToCpp(PyObject *object)

    Return the :c:type:`pkgAcquire::ItemDesc` pointer contained in the Python object
    *object*.

AcquireWorker (pkgAcquire::Worker)
----------------------------------
.. c:var:: PyTypeObject PyAcquireWorker_Type

    The type object for :class:`apt_pkg.AcquireWorker` objects.

.. c:function:: int PyAcquireWorker_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.AcquireWorker` object, or
    a subclass thereof.

.. c:function:: int PyAcquireWorker_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.AcquireWorker` object
    and no subclass thereof.

.. c:function:: PyObject* PyAcquireWorker_FromCpp(pkgAcquire::Worker *worker, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.AcquireWorker` object from the :c:type:`pkgAcquire::Worker`
    pointer given by the parameter *worker*. If the parameter *delete* is
    true, the object pointed to by *worker* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* should point
    to a :class:`apt_pkg.Acquire` object.

.. c:function:: pkgAcquire::Worker* PyAcquireWorker_ToCpp(PyObject *object)

    Return the :c:type:`pkgAcquire::Worker` pointer contained in the Python object
    *object*.

ActionGroup (pkgDepCache::ActionGroup)
--------------------------------------
.. c:var:: PyTypeObject PyActionGroup_Type

    The type object for :class:`apt_pkg.ActionGroup` objects.

.. c:function:: int PyActionGroup_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.ActionGroup` object, or
    a subclass thereof.

.. c:function:: int PyActionGroup_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.ActionGroup` object
    and no subclass thereof.

.. c:function:: PyObject* PyActionGroup_FromCpp(pkgDepCache::ActionGroup *agroup, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.ActionGroup` object from the :c:type:`pkgDepCache::ActionGroup`
    pointer given by the parameter *agroup*. If the parameter *delete* is
    true, the object pointed to by *agroup* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* should point
    to a :class:`apt_pkg.DepCache` object.

.. c:function:: pkgDepCache::ActionGroup* PyActionGroup_ToCpp(PyObject *object)

    Return the :c:type:`pkgDepCache::ActionGroup` pointer contained in the
    Python object *object*.

Cache (pkgCache)
------------------------
.. c:var:: PyTypeObject PyCache_Type

    The type object for :class:`apt_pkg.Cache` objects.

.. c:function:: int PyCache_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Cache` object, or
    a subclass thereof.

.. c:function:: int PyCache_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Cache` object
    and no subclass thereof.

.. c:function:: PyObject* PyCache_FromCpp(pkgCache *cache, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.Cache` object from the :c:type:`pkgCache`
    pointer given by the parameter *cache*. If the parameter *delete* is
    true, the object pointed to by *cache* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* shall point
    to a object of the type :c:data:`PyCacheFile_Type`.

.. c:function:: pkgCache* PyCache_ToCpp(PyObject *object)

    Return the :c:type:`pkgCache` pointer contained in the Python object
    *object*.


CacheFile (pkgCacheFile)
------------------------
.. c:var:: PyTypeObject PyCacheFile_Type

    The type object for CacheFile. This type is internal and not exported to
    Python anywhere.

.. c:function:: int PyCacheFile_Check(PyObject *object)

    Check that the object *object* is of the type :c:data:`PyCacheFile_Type` or
    a subclass thereof.

.. c:function:: int PyCacheFile_CheckExact(PyObject *object)

    Check that the object *object* is of the type :c:data:`PyCacheFile_Type` and
    no subclass thereof.

.. c:function:: PyObject* PyCacheFile_FromCpp(pkgCacheFile *file, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.CacheFile` object from the :c:type:`pkgCacheFile`
    pointer given by the parameter *file* If the parameter *delete* is
    true, the object pointed to by *file* will be deleted when the reference
    count of the returned object reaches 0.

.. c:function:: pkgCacheFile* PyCacheFile_ToCpp(PyObject *object)

    Return the :c:type:`pkgCacheFile` pointer contained in the Python object
    *object*.

Cdrom (pkgCdrom)
------------------------
.. c:var:: PyTypeObject PyCdrom_Type

    The type object for :class:`apt_pkg.Cdrom` objects.

.. c:function:: int PyCdrom_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Cdrom` object, or
    a subclass thereof.

.. c:function:: int PyCdrom_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Cdrom` object
    and no subclass thereof.

.. c:function:: PyObject* PyCdrom_FromCpp(pkgCdrom &cdrom, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.Cdrom` object from the :c:type:`pkgCdrom`
    reference given by the parameter *cdrom*. If the parameter *delete* is
    true, *cdrom* will be deleted when the reference count of the returned
    object reaches 0.

.. c:function:: pkgCdrom& PyCdrom_ToCpp(PyObject *object)

    Return the :c:type:`pkgCdrom` reference contained in the Python object
    *object*.

Configuration (Configuration)
-------------------------------
.. c:var:: PyTypeObject PyConfiguration_Type

    The type object for :class:`apt_pkg.Configuration` objects.

.. c:function:: int PyConfiguration_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Configuration` object, or
    a subclass thereof.

.. c:function:: int PyConfiguration_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Configuration` object
    and no subclass thereof.

.. c:function:: PyObject* PyConfiguration_FromCpp(Configuration *cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.Configuration` object from the :c:type:`Configuration`
    pointer given by the parameter *cpp*. If the parameter *delete* is
    true, the object pointed to by *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* may refer to
    a parent object (e.g. when exposing a sub tree of a configuration object).

.. c:function:: Configuration* PyConfiguration_ToCpp(PyObject *object)

    Return the :c:type:`Configuration` pointer contained in the Python object
    *object*.

DepCache (pkgDepCache)
------------------------
.. c:var:: PyTypeObject PyDepCache_Type

    The type object for :class:`apt_pkg.DepCache` objects.

.. c:function:: int PyDepCache_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.DepCache` object, or
    a subclass thereof.

.. c:function:: int PyDepCache_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.DepCache` object
    and no subclass thereof.

.. c:function:: PyObject* PyDepCache_FromCpp(pkgDepCache *cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.DepCache` object from the :c:type:`pkgDepCache`
    pointer given by the parameter *cpp*. If the parameter *delete* is
    true, the object pointed to by *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* must be
    a PyObject of the type :c:data:`PyCache_Type`.

.. c:function:: pkgDepCache* PyDepCache_ToCpp(PyObject *object)

    Return the :c:type:`pkgDepCache` pointer contained in the Python object
    *object*.

Dependency (pkgCache::DepIterator)
----------------------------------
.. c:var:: PyTypeObject PyDependency_Type

    The type object for :class:`apt_pkg.Dependency` objects.

.. c:function:: int PyDependency_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Dependency` object, or
    a subclass thereof.

.. c:function:: int PyDependency_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Dependency` object
    and no subclass thereof.

.. c:function:: PyObject* PyDependency_FromCpp(pkgCache::DepIterator &cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.Dependency` object from the :c:type:`pkgCache::DepIterator`
    reference given by the parameter *cpp*. If the parameter *delete* is
    true, *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* must be
    a PyObject of the type :c:data:`PyPackage_Type`.

.. c:function:: pkgCache::DepIterator& PyDependency_ToCpp(PyObject *object)

    Return the :c:type:`pkgCache::DepIterator` reference contained in the
    Python object *object*.

Description (pkgCache::DescIterator)
------------------------------------
.. c:var:: PyTypeObject PyDescription_Type

    The type object for :class:`apt_pkg.Description` objects.

.. c:function:: int PyDescription_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Description` object, or
    a subclass thereof.

.. c:function:: int PyDescription_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Description` object
    and no subclass thereof.

.. c:function:: PyObject* PyDescription_FromCpp(pkgCache::DescIterator &cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.Description` object from the :c:type:`pkgCache::DescIterator`
    reference given by the parameter *cpp*. If the parameter *delete* is
    true, *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* must be
    a PyObject of the type :c:data:`PyPackage_Type`.

.. c:function:: pkgCache::DescIterator& PyDescription_ToCpp(PyObject *object)

    Return the :c:type:`pkgCache::DescIterator` reference contained in the
    Python object *object*.


Group (pkgCache::GrpIterator)
----------------------------------
.. versionadded:: 0.8.0

.. c:var:: PyTypeObject PyGroup_Type

    The type object for :class:`apt_pkg.Group` objects.

.. c:function:: int PyGroup_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Group` object, or
    a subclass thereof.

.. c:function:: int PyGroup_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Group` object
    and no subclass thereof.

.. c:function:: PyObject* PyGroup_FromCpp(pkgCache::GrpIterator &cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.Group` object from the :c:type:`pkgCache::GrpIterator`
    reference given by the parameter *cpp*. If the parameter *delete* is
    true, *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* should be
    a PyObject of the type :c:data:`PyCache_Type`.

.. c:function:: pkgCache::GrpIterator& PyGroup_ToCpp(PyObject *object)

    Return the :c:type:`pkgCache::GrpIterator` reference contained in the
    Python object *object*.

Hashes (Hashes)
----------------------------------
.. c:var:: PyTypeObject PyHashes_Type

    The type object for :class:`apt_pkg.Hashes` objects.

.. c:function:: int PyHashes_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Hashes` object, or
    a subclass thereof.

.. c:function:: int PyHashes_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Hashes` object
    and no subclass thereof.

.. c:function:: PyObject* PyHashes_FromCpp(Hashes &cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.Hashes` object from the :c:type:`Hashes`
    reference given by the parameter *cpp*. If the parameter *delete* is
    true, *cpp* will be deleted when the reference count of the returned
    object reaches 0.

.. c:function:: Hashes& PyHashes_ToCpp(PyObject *object)

    Return the :c:type:`Hashes` reference contained in the
    Python object *object*.

HashString (HashString)
------------------------
.. c:var:: PyTypeObject PyHashString_Type

    The type object for :class:`apt_pkg.HashString` objects.

.. c:function:: int PyHashString_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.HashString` object, or
    a subclass thereof.

.. c:function:: int PyHashString_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.HashString` object
    and no subclass thereof.

.. c:function:: PyObject* PyHashString_FromCpp(HashString *cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.HashString` object from the :c:type:`HashString`
    pointer given by the parameter *cpp*. If the parameter *delete* is
    true, the object pointed to by *cpp* will be deleted when the reference
    count of the returned object reaches 0.

.. c:function:: HashString* PyHashString_ToCpp(PyObject *object)

    Return the :c:type:`HashString` pointer contained in the Python object
    *object*.

IndexRecords (indexRecords)
----------------------------
.. c:var:: PyTypeObject PyIndexRecords_Type

    The type object for :class:`apt_pkg.IndexRecords` objects.

.. c:function:: int PyIndexRecords_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.IndexRecords` object, or
    a subclass thereof.

.. c:function:: int PyIndexRecords_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.IndexRecords` object
    and no subclass thereof.

.. c:function:: PyObject* PyIndexRecords_FromCpp(indexRecords *cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.IndexRecords` object from the :c:type:`indexRecords`
    pointer given by the parameter *cpp*. If the parameter *delete* is
    true, the object pointed to by *cpp* will be deleted when the reference
    count of the returned object reaches 0.

.. c:function:: indexRecords* PyIndexRecords_ToCpp(PyObject *object)

    Return the :c:type:`indexRecords` pointer contained in the Python object
    *object*.


MetaIndex (metaIndex)
------------------------
.. c:var:: PyTypeObject PyMetaIndex_Type

    The type object for :class:`apt_pkg.MetaIndex` objects.

.. c:function:: int PyMetaIndex_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.MetaIndex` object, or
    a subclass thereof.

.. c:function:: int PyMetaIndex_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.MetaIndex` object
    and no subclass thereof.

.. c:function:: PyObject* PyMetaIndex_FromCpp(metaIndex *cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.MetaIndex` object from the :c:type:`metaIndex`
    pointer given by the parameter *cpp*. If the parameter *delete* is
    true, the object pointed to by *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* should be
    a PyObject of the type :c:data:`PySourceList_Type`.

.. c:function:: metaIndex* PyMetaIndex_ToCpp(PyObject *object)

    Return the :c:type:`metaIndex` pointer contained in the Python object
    *object*.

Package (pkgCache::PkgIterator)
----------------------------------
.. c:var:: PyTypeObject PyPackage_Type

    The type object for :class:`apt_pkg.Package` objects.

.. c:function:: int PyPackage_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Package` object, or
    a subclass thereof.

.. c:function:: int PyPackage_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Package` object
    and no subclass thereof.

.. c:function:: PyObject* PyPackage_FromCpp(pkgCache::PkgIterator &cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.Package` object from the :c:type:`pkgCache::PkgIterator`
    reference given by the parameter *cpp*. If the parameter *delete* is
    true, *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* should be
    a PyObject of the type :c:data:`PyCache_Type`.

.. c:function:: pkgCache::PkgIterator& PyPackage_ToCpp(PyObject *object)

    Return the :c:type:`pkgCache::PkgIterator` reference contained in the
    Python object *object*.

PackageFile (pkgCache::PkgFileIterator)
----------------------------------------
.. c:var:: PyTypeObject PyPackageFile_Type

    The type object for :class:`apt_pkg.PackageFile` objects.

.. c:function:: int PyPackageFile_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.PackageFile` object, or
    a subclass thereof.

.. c:function:: int PyPackageFile_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.PackageFile` object
    and no subclass thereof.

.. c:function:: PyObject* PyPackageFile_FromCpp(pkgCache::PkgFileIterator &cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.PackageFile` object from the :c:type:`pkgCache::PkgFileIterator`
    reference given by the parameter *cpp*. If the parameter *delete* is
    true, *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* should be
    a PyObject of the type :c:data:`PyCache_Type`.

.. c:function:: pkgCache::PkgFileIterator& PyPackageFile_ToCpp(PyObject *object)

    Return the :c:type:`pkgCache::PkgFileIterator` reference contained in the
    Python object *object*.

IndexFile (pkgIndexFile)
--------------------------------------
.. c:var:: PyTypeObject PyIndexFile_Type

    The type object for :class:`apt_pkg.IndexFile` objects.

.. c:function:: int PyIndexFile_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.IndexFile` object, or
    a subclass thereof.

.. c:function:: int PyIndexFile_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.IndexFile` object
    and no subclass thereof.

.. c:function:: PyObject* PyIndexFile_FromCpp(pkgIndexFile *cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.IndexFile` object from the :c:type:`pkgIndexFile`
    pointer given by the parameter *cpp*. If the parameter *delete* is
    true, the object pointed to by *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* should be
    a PyObject of the type :c:data:`PyMetaIndex_Type`.

.. c:function:: pkgIndexFile* PyIndexFile_ToCpp(PyObject *object)

    Return the :c:type:`pkgIndexFile` pointer contained in the Python object
    *object*.

OrderList (pkgOrderList)
---------------------------
.. c:var:: PyTypeObject PyOrderList_Type

    The type object for :class:`apt_pkg.OrderList` objects.

.. c:function:: int PyOrderList_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.OrderList` object, or
    a subclass thereof.

.. c:function:: int PyOrderList_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.OrderList` object
    and no subclass thereof.

.. c:function:: PyObject* PyOrderList_FromCpp(pkgOrderList *cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.OrderList` object from the :c:type:`pkgOrderList`
    pointer given by the parameter *cpp*. If the parameter *delete* is
    true, the object pointed to by *cpp* will be deleted when the reference
    count of the returned object reaches 0. The owner must be a
    :class:`apt_pkg.DepCache` object.

.. c:function:: pkgOrderList* PyOrderList_ToCpp(PyObject *object)

    Return the :c:type:`pkgOrderList` pointer contained in the Python object
    *object*.

PackageManager (pkgPackageManager)
----------------------------------
.. c:var:: PyTypeObject PyPackageManager_Type

    The type object for :class:`apt_pkg.PackageManager` objects.

.. c:function:: int PyPackageManager_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.PackageManager` object, or
    a subclass thereof.

.. c:function:: int PyPackageManager_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.PackageManager` object
    and no subclass thereof.

.. c:function:: PyObject* PyPackageManager_FromCpp(pkgPackageManager *cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.PackageManager` object from the :c:type:`pkgPackageManager`
    pointer given by the parameter *cpp*. If the parameter *delete* is
    true, the object pointed to by *cpp* will be deleted when the reference
    count of the returned object reaches 0.

.. c:function:: pkgPackageManager* PyPackageManager_ToCpp(PyObject *object)

    Return the :c:type:`pkgPackageManager` pointer contained in the Python object
    *object*.


Policy (pkgPolicy)
------------------
.. c:var:: PyTypeObject PyPolicy_Type

    The type object for :class:`apt_pkg.Policy` objects.

.. c:function:: int PyPolicy_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Policy` object, or
    a subclass thereof.

.. c:function:: int PyPolicy_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Policy` object
    and no subclass thereof.

.. c:function:: PyObject* PyPolicy_FromCpp(pkgPolicy *cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.Policy` object from the :c:type:`pkgPolicy`
    pointer given by the parameter *cpp*. If the parameter *delete* is
    true, the object pointed to by *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* must be
    a PyObject of the type :c:data:`PyCache_Type`.

.. c:function:: pkgPolicy* PyPolicy_ToCpp(PyObject *object)

    Return the :c:type:`pkgPolicy` pointer contained in the Python object
    *object*.


ProblemResolver (pkgProblemResolver)
--------------------------------------
.. c:var:: PyTypeObject PyProblemResolver_Type

    The type object for :class:`apt_pkg.ProblemResolver` objects.

.. c:function:: int PyProblemResolver_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.ProblemResolver` object, or
    a subclass thereof.

.. c:function:: int PyProblemResolver_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.ProblemResolver` object
    and no subclass thereof.

.. c:function:: PyObject* PyProblemResolver_FromCpp(pkgProblemResolver *cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.ProblemResolver` object from the :c:type:`pkgProblemResolver`
    pointer given by the parameter *cpp*. If the parameter *delete* is
    true, the object pointed to by *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* must be
    a PyObject of the type :c:data:`PyDepCache_Type`.

.. c:function:: pkgProblemResolver* PyProblemResolver_ToCpp(PyObject *object)

    Return the :c:type:`pkgProblemResolver` pointer contained in the Python object
    *object*.



SourceList (pkgSourceList)
---------------------------
.. c:var:: PyTypeObject PySourceList_Type

    The type object for :class:`apt_pkg.SourceList` objects.

.. c:function:: int PySourceList_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.SourceList` object, or
    a subclass thereof.

.. c:function:: int PySourceList_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.SourceList` object
    and no subclass thereof.

.. c:function:: PyObject* PySourceList_FromCpp(pkgSourceList *cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.SourceList` object from the :c:type:`pkgSourceList`
    pointer given by the parameter *cpp*. If the parameter *delete* is
    true, the object pointed to by *cpp* will be deleted when the reference
    count of the returned object reaches 0.

.. c:function:: pkgSourceList* PySourceList_ToCpp(PyObject *object)

    Return the :c:type:`pkgSourceList` pointer contained in the Python object
    *object*.


TagFile (pkgTagFile)
----------------------------------
.. c:var:: PyTypeObject PyTagFile_Type

    The type object for :class:`apt_pkg.TagFile` objects.

.. c:function:: int PyTagFile_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.TagFile` object, or
    a subclass thereof.

.. c:function:: int PyTagFile_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.TagFile` object
    and no subclass thereof.

.. c:function:: PyObject* PyTagFile_FromCpp(pkgTagFile &cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.TagFile` object from the :c:type:`pkgTagFile`
    reference given by the parameter *cpp*. If the parameter *delete* is
    true, *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* may be any
    Python object.

.. c:function:: pkgTagFile& PyTagFile_ToCpp(PyObject *object)

    Return the :c:type:`pkgTagFile` reference contained in the
    Python object *object*.

TagSection (pkgTagSection)
----------------------------------
.. c:var:: PyTypeObject PyTagSection_Type

    The type object for :class:`apt_pkg.TagSection` objects.

.. c:function:: int PyTagSection_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.TagSection` object, or
    a subclass thereof.

.. c:function:: int PyTagSection_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.TagSection` object
    and no subclass thereof.

.. c:function:: PyObject* PyTagSection_FromCpp(pkgTagSection &cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.TagSection` object from the :c:type:`pkgTagSection`
    reference given by the parameter *cpp*. If the parameter *delete* is
    true, *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* may be
    a PyObject of the type :c:data:`PyTagFile_Type`.

.. c:function:: pkgTagSection& PyTagSection_ToCpp(PyObject *object)

    Return the :c:type:`pkgTagSection` reference contained in the
    Python object *object*.

Version (pkgCache::VerIterator)
----------------------------------
.. c:var:: PyTypeObject PyVersion_Type

    The type object for :class:`apt_pkg.Version` objects.

.. c:function:: int PyVersion_Check(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Version` object, or
    a subclass thereof.

.. c:function:: int PyVersion_CheckExact(PyObject *object)

    Check that the object *object* is an :class:`apt_pkg.Version` object
    and no subclass thereof.

.. c:function:: PyObject* PyVersion_FromCpp(pkgCache::VerIterator &cpp, bool delete, PyObject *owner)

    Create a new :class:`apt_pkg.Version` object from the :c:type:`pkgCache::VerIterator`
    reference given by the parameter *cpp*. If the parameter *delete* is
    true, *cpp* will be deleted when the reference
    count of the returned object reaches 0. The parameter *owner* must be
    a PyObject of the type :c:data:`PyPackage_Type`.

.. c:function:: pkgCache::VerIterator& PyVersion_ToCpp(PyObject *object)

    Return the :c:type:`pkgCache::VerIterator` reference contained in the
    Python object *object*.
