/*
This, combined with `CLANG_ENABLE_MODULES` in libplayer_core
will add `LC_LINKER_OPTION` auto linker option helper to library
To check, run:

otool -l libplayer.a | grep -A 4 LC_LINKER_OPTION | grep string | grep -v '\-framework' | awk '{ print $3 }' | sort | uniq

*/

#import <AssetsLibrary/AssetsLibrary.h>
#import <AVKit/AVKit.h>
#import <CoreLocation/CoreLocation.h>
#import <CoreMotion/CoreMotion.h>
#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#ifndef Rtt_MetalANGLE
#import <GLKit/GLKit.h>
#else
/* MetalANGLE requires modules; skipped with CLANG_ENABLE_MODULES=NO */
#endif
#import <MapKit/MapKit.h>
#import <MessageUI/MessageUI.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <OpenAL/al.h>
#import <Photos/Photos.h>
#import <sqlite3.h>
#import <StoreKit/StoreKit.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <WebKit/WebKit.h>
