// Copyright (c) 2011-2013 The Fittexxcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "macnotificationhandler.h"

#undef slots
#include <Cocoa/Cocoa.h>
#import <objc/runtime.h>

// Add an obj-c category (extension) to return the expected bundle identifier
@implementation NSBundle (returnCorrectIdentifier)
- (NSString *)__bundleIdentifier {
    if (self == [NSBundle mainBundle]) {
        return @"org.fittexxcoinnode.FittexxcoinNode-Qt";
    } else {
        return [self __bundleIdentifier];
    }
}
@end

void MacNotificationHandler::showNotification(const QString &title,
                                              const QString &text) {
    // check if users OS has support for NSUserNotification
    if (this->hasUserNotificationCenterSupport()) {
        // okay, seems like 10.8+
        QByteArray utf8 = title.toUtf8();
        char *cString = (char *)utf8.constData();
        NSString *titleMac = [[NSString alloc] initWithUTF8String:cString];

        utf8 = text.toUtf8();
        cString = (char *)utf8.constData();
        NSString *textMac = [[NSString alloc] initWithUTF8String:cString];

        // do everything weak linked (because we will keep <10.8 compatibility)
        id userNotification =
            [[NSClassFromString(@"NSUserNotification") alloc] init];
        [userNotification performSelector:@selector(setTitle:)
                               withObject:titleMac];
        [userNotification performSelector:@selector(setInformativeText:)
                               withObject:textMac];

        id notificationCenterInstance =
            [NSClassFromString(@"NSUserNotificationCenter")
                performSelector:@selector(defaultUserNotificationCenter)];
        [notificationCenterInstance
            performSelector:@selector(deliverNotification:)
                 withObject:userNotification];

        [titleMac release];
        [textMac release];
        [userNotification release];
    }
}

bool MacNotificationHandler::hasUserNotificationCenterSupport() {
    Class possibleClass = NSClassFromString(@"NSUserNotificationCenter");

    // check if users OS has support for NSUserNotification
    if (possibleClass != nil) {
        return true;
    }
    return false;
}

MacNotificationHandler *MacNotificationHandler::instance() {
    static MacNotificationHandler *s_instance = nullptr;
    if (!s_instance) {
        s_instance = new MacNotificationHandler();

        Class aPossibleClass = objc_getClass("NSBundle");
        if (aPossibleClass) {
            // change NSBundle -bundleIdentifier method to return a correct
            // bundle identifier a bundle identifier is required to use OSXs
            // User Notification Center
            method_exchangeImplementations(
                class_getInstanceMethod(aPossibleClass,
                                        @selector(bundleIdentifier)),
                class_getInstanceMethod(aPossibleClass,
                                        @selector(__bundleIdentifier)));
        }
    }
    return s_instance;
}
