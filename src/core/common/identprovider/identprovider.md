# Ident provider

It provides abstractions for observing, publishing, and handling identity-related subjects, system IDs, and unit models.

* [aos::identprovider::SubjectsObserverItf](itf/identprovider.hpp) - receives subject change notifications;
* [aos::identprovider::IdentProviderItf](itf/identprovider.hpp) - manages identity-related information.

## aos::identprovider::SubjectsObserverItf

### SubjectsChanged

Called when subjects change.

## aos::identprovider::IdentProviderItf

### GetSystemID

Retrieves the system ID.

### GetUnitModel

Retrieves the unit model.

### GetSubjects

Retrieves the list of subjects.

### SubscribeSubjectsChanged

Subscribes an observer to subject change notifications.

### UnsubscribeSubjectsChanged

Unsubscribes an observer from subject change notifications.
