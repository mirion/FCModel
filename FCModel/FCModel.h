//
//  FCModel.h
//
//  Created by Marco Arment on 7/18/13.
//  Copyright (c) 2013-2014 Marco Arment. See included LICENSE file.
//

#import <Foundation/Foundation.h>
#include <AvailabilityMacros.h>

#ifdef COCOAPODS
#import <FMDB/FMDatabase.h>
#else
#import "FMDatabase.h"
#endif

@class FCModelFieldInfo;

// These notifications use the relevant model's Class as the "object" for convenience so observers can,
//  for instance, observe every update to any instance of the Person class:
//
//  [NSNotificationCenter.defaultCenter addObserver:... selector:... name:FCModelUpdateNotification object:Person.class];
//
// Or set object to nil to get notified of operations to all FCModels.
//
extern NSString * _Nonnull const FCModelInsertNotification;
extern NSString * _Nonnull const FCModelUpdateNotification;
extern NSString * _Nonnull const FCModelDeleteNotification;
extern NSString * _Nonnull const FCModelAnyChangeNotification; // Any insert, update, delete, dataWasUpdatedExternally, or executeUpdateQuery:.
//
// userInfo[FCModelInstanceSetKey] is an NSSet containing the specific FCModel instance(s) acted upon.
// The set will always contain exactly one instance, except:
//  - If you use begin/endNotificationBatchAndNotify, it will contain all instances that received the notification during the batch.
//  - For dataWasUpdatedExternally/executeUpdateQuery:, it will contain all loaded instances of the class.
//
extern NSString * _Nonnull const FCModelInstanceSetKey;
//
// userInfo[FCModelChangedFieldsByInstanceKey] is an NSSet of NSString field names.
// "Changed" field names may be overly inclusive: all named fields may not *actually* have changed, but all actual changes will be in the set.
//
extern NSString * _Nonnull const FCModelChangedFieldsKey;


// During dataWasUpdatedExternally and executeUpdateQuery:, this is called immediately before FCModel tells all loaded
//  instances of the affected class to reload themselves. Reloading can be time-consuming if many instances are in memory,
//  so this is a good time to release any unnecessarily retained instances so they don't need to go through the reload.
// The notification's object is the affected class.
//
// (You probably don't need to care about this. Until you do.)
//
extern NSString * _Nonnull const FCModelWillReloadNotification;


typedef NS_ENUM(NSInteger, FCModelSaveResult) {
    FCModelSaveFailed = 0, // SQLite refused a query. Check .lastSQLiteError
    FCModelSaveRefused,    // The instance blocked the operation from a should* method.
    FCModelSaveSucceeded,
    FCModelSaveNoChanges
};

@interface FCModel : NSObject

@property (nullable, readonly) id primaryKey;
@property (nonnull, readonly) NSDictionary< NSString *, NSObject * > *allFields;
@property (readonly) BOOL hasUnsavedChanges;
@property (readonly) BOOL existsInDatabase; // either deleted or never saved
@property (readonly) BOOL isDeleted;
@property (nullable, readonly) NSError *lastSQLiteError;

+ (void)registerCustomModel; // Use this method to register non-standard models, i.e. models that are mapped to tables/views not having exactly the same name
+ (void)openDatabaseAtPath:(NSString * _Nonnull)path withSchemaBuilder:(nonnull void (^)(FMDatabase * _Nonnull db, int * _Nonnull schemaVersion))schemaBuilder;
+ (void)openDatabaseAtPath:(NSString * _Nonnull)path withDatabaseInitializer:(nullable void (^)(FMDatabase * _Nonnull db))databaseInitializer schemaBuilder:(nonnull void (^)(FMDatabase * _Nonnull db, int * _Nonnull schemaVersion))schemaBuilder;

+ (nullable NSArray *)databaseFieldNames;
+ (nullable NSString *)primaryKeyFieldName;
+ (nullable NSString *)tableName;

// Be careful with this -- the array could be out of date by the time you use it
//  if a new instance is loaded by another thread. Everything in it is guaranteed
//  to be a loaded instance, but you're not guaranteed to always have *all* of them
//  if you perform SELECTs from multiple threads.
+ (nonnull NSArray *)allLoadedInstances;

// Feel free to operate on the same database object with your own queries. They'll be
//  executed synchronously on FCModel's private database-operation queue.
//  (IMPORTANT: READ THE NEXT METHOD DEFINITION)
+ (void)inDatabaseSync:(nonnull void (^)(FMDatabase * _Nonnull db))block;
+ (void)inDatabaseSyncUpdate:(nonnull void (^)(FMDatabase * _Nonnull db))block;

// Call if you perform INSERT/UPDATE/DELETE on any FCModel table outside of the instance*/save
// methods. This will cause any instances in existence to reload their data from the database.
//
//  - Call on a subclass to reload all instances of that model and any subclasses.
//  - Call on FCModel to reload all instances of ALL models.
//
+ (void)dataWasUpdatedExternally;

// Or use one of these convenience methods, which calls dataWasUpdatedExternally automatically and offers $T/$PK parsing.
// If you don't know which tables will be affected, or if it will affect more than one, call on FCModel, not a subclass.
// Only call on a subclass if only that model's table will be affected.
// By default the instances currently loaded are notified in order to reload their data
// If you don't want to notify them, use the notify argument set to NO
+ (nullable NSError *)executeUpdateQuery:(nonnull NSString *)query, ...;
+ (nullable NSError *)executeUpdateQuery:(nonnull NSString *)query notify:(BOOL)notify, ...;
+ (nullable NSError *)executeUpdateQuery:(nonnull NSString *)query arguments:(nullable NSArray *)arguments;
+ (nullable NSError *)executeUpdateQuery:(nonnull NSString *)query notify:(BOOL)notify arguments:(nullable NSArray *)arguments;

// CRUD basics
+ (nullable instancetype)instanceWithPrimaryKey:(nullable id)primaryKeyValue; // will create if nonexistent
+ (nullable instancetype)instanceWithPrimaryKey:(nullable id)primaryKeyValue createIfNonexistent:(BOOL)create; // will return nil if nonexistent
+ (nullable instancetype)autonomousInstanceFromDatabaseWithPrimaryKey:(nullable id)key; // will return an instance as it is stored into the database. The result will not be cached
- (nonnull NSArray *)changedFieldNames;
- (void)revertUnsavedChanges;
- (void)revertUnsavedChangeToFieldName:(nonnull NSString *)fieldName;
- (void)reload;
- (void)reloadAfterRevertUnsavedChanges;
- (FCModelSaveResult)delete;
- (FCModelSaveResult)save;
+ (void)saveAll; // Resolved by class: call on FCModel to save all, on a subclass to save just those and their subclasses, etc.

// SELECTs
// - "keyed" variants return dictionaries keyed by each instance's primary-key value.
// - "FromResultSet" variants will iterate through the supplied result set, but the caller is still responsible for closing it.
// - Optional query placeholders:
//      $T  - This model's table name
//      $PK - This model's primary-key field name
//
+ (nullable NSArray *)allInstances;
+ (nullable NSDictionary *)keyedAllInstances;

+ (nullable NSArray *)instancesFromResultSet:(nonnull FMResultSet *)rs;
+ (nullable NSDictionary *)keyedInstancesFromResultSet:(nonnull FMResultSet *)rs;
+ (nullable instancetype)firstInstanceFromResultSet:(nonnull FMResultSet *)rs;

+ (nullable instancetype)firstInstanceWhere:(nonnull NSString *)queryAfterWHERE, ...;
+ (nullable instancetype)firstInstanceWhere:(nonnull NSString *)queryAfterWHERE arguments:(nullable NSArray *)arguments;
+ (nullable NSArray *)instancesWhere:(nonnull NSString *)queryAfterWHERE, ...;
+ (nullable NSArray *)instancesWhere:(nonnull NSString *)queryAfterWHERE arguments:(nullable NSArray *)array;
+ (nullable NSDictionary *)keyedInstancesWhere:(nonnull NSString *)queryAfterWHERE, ...;
+ (nullable NSDictionary *)keyedInstancesWhere:(nonnull NSString *)queryAfterWHERE arguments:(nullable NSArray *)arguments;

+ (nullable instancetype)firstInstanceOrderedBy:(nonnull NSString *)queryAfterORDERBY, ...;
+ (nullable instancetype)firstInstanceOrderedBy:(nonnull NSString *)queryAfterORDERBY arguments:(nullable NSArray *)arguments;
+ (nullable NSArray *)instancesOrderedBy:(nonnull NSString *)queryAfterORDERBY, ...;
+ (nullable NSArray *)instancesOrderedBy:(nonnull NSString *)queryAfterORDERBY arguments:(nullable NSArray *)arguments;

+ (NSUInteger)numberOfInstances;
+ (NSUInteger)numberOfInstancesWhere:(nonnull NSString *)queryAfterWHERE, ...;
+ (NSUInteger)numberOfInstancesWhere:(nonnull NSString *)queryAfterWHERE arguments:(nullable NSArray *)arguments;

// Fetch a set of primary keys, i.e. "WHERE key IN (...)"
+ (nullable NSArray *)instancesWithPrimaryKeyValues:(nonnull NSArray *)primaryKeyValues;
+ (nullable NSDictionary *)keyedInstancesWithPrimaryKeyValues:(nonnull NSArray *)primaryKeyValues;

// Return data instead of completed objects (convenient accessors to FCModel's database queue with $T/$PK parsing)
+ (nullable NSArray *)resultDictionariesFromQuery:(nonnull NSString *)query, ...;
+ (nullable NSArray *)resultDictionariesFromQuery:(nonnull NSString *)query arguments:(nullable NSArray *)arguments;
+ (nullable NSArray *)firstColumnArrayFromQuery:(nonnull NSString *)query, ...;
+ (nullable NSArray *)firstColumnArrayFromQuery:(nonnull NSString *)query arguments:(nullable NSArray *)arguments;
+ (nullable id)firstValueFromQuery:(nonnull NSString *)query, ...;
+ (nullable id)firstValueFromQuery:(nonnull NSString *)query arguments:(nullable NSArray *)arguments;

// These methods use a global query cache (in FCModelCachedObject). Results are cached indefinitely until their
//  table has any writes or there's a system low-memory warning, at which point they automatically invalidate.
//  You can customize whether invalidations are triggered with the optional ignoreFieldsForInvalidation: params.
// The next subsequent request will repopulate the cached data, either by querying the DB (cachedInstancesWhere)
//  or calling the generator block (cachedObjectWithIdentifier).
//
+ (nullable NSArray *)cachedInstancesWhere:(nonnull NSString *)queryAfterWHERE arguments:(nullable NSArray *)arguments;
+ (nullable NSArray *)cachedInstancesWhere:(nonnull NSString *)queryAfterWHERE arguments:(nullable NSArray *)arguments ignoreFieldsForInvalidation:(nullable NSSet *)ignoredFields;
+ (nullable id)cachedObjectWithIdentifier:(nonnull id)identifier generator:(nonnull id _Nonnull (^)(void))generatorBlock;
+ (nullable id)cachedObjectWithIdentifier:(nonnull id)identifier ignoreFieldsForInvalidation:(nullable NSSet *)ignoredFields generator:(nonnull id _Nonnull (^)(void))generatorBlock;

// For subclasses to override, all optional:

- (void)didInit;
- (BOOL)shouldInsert;
- (BOOL)shouldUpdate;
- (BOOL)shouldDelete;
- (void)didInsert;
- (void)didUpdate;
- (void)didDelete;
- (void)saveWasRefused;
- (void)saveDidFail;

+ (nonnull NSSet *)ignoredFieldNames; // Fields that exist in the table but should not be read into the model. Default empty set, cannot be nil.

+ (BOOL)useInstancesCache; // Return true if the instances should be cached into the memory

// Implement this method if you want to use another column as primary key
// This is necessary in order to map FCModel instances on views
+ (nullable NSString *)configuredPrimaryKeyName;
+ (nullable NSString *)configuredTableName; // Implement this method if you want to use a custom table mapping. In order to be known by FCModel, this subclass should be registered using registerCustomModel.

// To create new records with supplied primary-key values, call instanceWithPrimaryKey:, then save when done
//  setting other fields.
//
// This method is only called if you call +new to create a new instance with an automatic primary-key value.
//
// By default, this method generates random int64_t values. Subclasses may override it to e.g. use UUID strings
//  or other values, but the values must be unique within the table. If you return something that already exists
//  in the table or in an unsaved in-memory instance, FCModel will keep calling this up to 100 times looking for
//  a unique value before raising an exception.
//
+ (nonnull id)primaryKeyValueForNewInstance;

// Subclasses can customize how properties are serialized for the database.
//
// FCModel automatically handles numeric primitives, NSString, NSNumber, NSData, NSURL, NSDate, NSDictionary, and NSArray.
// (Note that NSDate is stored as a time_t, so values before 1970 won't serialize properly.)
//
// To override this behavior or customize it for other types, you can implement these methods.
// You MUST call the super implementation for values that you're not handling.
//
// Database values may be NSString or NSNumber for INTEGER/FLOAT/TEXT columns, or NSData for BLOB columns.
//
- (nullable id)serializedDatabaseRepresentationOfValue:(nullable id)instanceValue forPropertyNamed:(nonnull NSString *)propertyName;
- (nullable id)unserializedRepresentationOfDatabaseValue:(nullable id)databaseValue forPropertyNamed:(nonnull NSString *)propertyName;

// Called on subclasses if there's a reload conflict:
//  - The instance changes field X but doesn't save the changes to the database.
//  - Database updates are executed outside of FCModel that cause instances to reload their data.
//  - This instance's value for field X in the database is different from the unsaved value it has.
//
// The default implementation raises an exception, so implement this if you use +dataWasUpdatedExternally or +executeUpdateQuery,
//  and don't call super.
//
- (nullable id)valueOfFieldName:(nonnull NSString *)fieldName byResolvingReloadConflictWithDatabaseValue:(nullable id)valueInDatabase;

// Notification batches and queuing:
//
// A common pattern is to listen for FCModelInsert/Update/DeleteNotification and reload a table or take other expensive UI operations.
// When small numbers of instances are updated/deleted during normal use, that's fine. But when doing a large operation in which
//  hundreds or thousands of instances might be changed, responding to these notifications may cause noticeable performance problems.
//
// Using this batch-queuing system, you can temporarily suspend delivery of these notifications, then deliver or discard them.
// Multiple identical notification types for each class will be collected into one. For instance:
//
// Without notification batching:
//
//     FCModelInsertNotification: Person class, { Sue }
//     FCModelUpdateNotification: Person class, { Robert }
//     FCModelUpdateNotification: Person class, { Sarah }
//     FCModelUpdateNotification: Person class, { James }
//     FCModelUpdateNotification: Person class, { Kate }
//     FCModelDeleteNotification: Person class, { Richard }
//
// With notification batching:
//
//     FCModelInsertNotification: Person class, { Sue }
//     FCModelUpdateNotification: Person class, { Robert, Sarah, James, Kate }
//     FCModelDeleteNotification: Person class, { Richard }
//
// Be careful: batch notification order is not preserved, and you may be unexpectedly interacting with deleted instances.
// Always check the given instances' .existsInDatabase property.
//
// NOTE: Notification batching is thread-local. Operations performed in other threads will still send notifications normally.
//
+ (void)performWithBatchedNotifications:(nonnull void (^)())block deliverOnCompletion:(BOOL)deliverNotifications;
+ (void)performWithBatchedNotifications:(nonnull void (^)())block; // equivalent to performWithBatchedNotifications:deliverOnCompletion:YES
+ (BOOL)isBatchingNotificationsForCurrentThread;

// Field info: You probably won't need this most of the time, but it's nice to have sometimes. FCModel's generating this privately
//  anyway, so you might as well have read-only access to it if it can help you avoid some code. (I've already needed it.)
//
+ (nullable FCModelFieldInfo *)infoForFieldName:(nonnull NSString *)fieldName;

// Clear the data loaded in memory
+ (void)clearCachedData;

// Closing the database is not necessary in most cases. Only close it if you need to, such as if you need to delete and recreate
//  the database file. Caveats:
//     - Any FCModel call after closing will bizarrely fail until you call openDatabaseAtPath: again.
//     - Any FCModel instances retained by any other parts of your code at the time of closing will become abandoned and untracked.
//        The uniqueness guarantee will be broken, and operations on those instances will have undefined behavior. You really don't
//        want this, and it may raise an exception in the future.
//
//        Until then, having any resident FCModel instances at the time of closing the database will result in scary console warnings
//        and a return value of NO, which you should take as a condescending judgment and should fix immediately.
//
// Returns YES if there were no resident FCModel instances.
//
+ (BOOL)closeDatabase;

// If you try to use FCModel while the database is closed, an error will be logged to the console on any relevant calls.
// Read/info/SELECT methods will return nil when possible, but these will throw exceptions:
//  -save
//  +saveAll
//  -delete
//  -executeUpdateQuery:
//  -inDatabaseSync:
//
// You can determine if the database is currently open:
//
+ (BOOL)databaseIsOpen;

@end


typedef NS_ENUM(NSInteger, FCModelFieldType) {
    FCModelFieldTypeOther = 0,
    FCModelFieldTypeText,
    FCModelFieldTypeInteger,
    FCModelFieldTypeDouble,
    FCModelFieldTypeBool
};

@interface FCModelFieldInfo : NSObject
@property (nonatomic, readonly) BOOL nullAllowed;
@property (nonatomic, readonly) FCModelFieldType type;
@property (nullable, nonatomic, readonly) id defaultValue;
@property (nullable, nonatomic, readonly) Class propertyClass;
@property (nullable, nonatomic, readonly) NSString *propertyTypeEncoding;
@end

