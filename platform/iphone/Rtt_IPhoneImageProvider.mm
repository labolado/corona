//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"
#include "Rtt_Lua.h"

#include "Rtt_IPhoneImageProvider.h"
#include "Rtt_IPhoneMediaProvider.h"
#include "Rtt_AppleBitmap.h"
#include "Rtt_AppleData.h"
#include "Rtt_Runtime.h"
#include "Rtt_RenderingStream.h"

#import <AssetsLibrary/AssetsLibrary.h>
#import <AVFoundation/AVFoundation.h>
#import <Photos/Photos.h>
#import <UIKit/UIKit.h>
#import <MediaPlayer/MediaPlayer.h>
#import <PhotosUI/PhotosUI.h>

#import "AppDelegate.h"
#import <MobileCoreServices/MobileCoreServices.h>


// Callback/Notification glue code
@interface IPhoneImagePickerControllerDelegate : NSObject< UIImagePickerControllerDelegate >
{
	Rtt::IPhoneImageProvider* callback;
}

@property (nonatomic, readwrite, assign) Rtt::IPhoneImageProvider* callback;

@end

@implementation IPhoneImagePickerControllerDelegate

@synthesize callback;
- (void)imagePickerController:(UIImagePickerController *)picker didFinishPickingMediaWithInfo:(NSDictionary *)info
{
	if ( picker )
	{
		UIImage *pickedImage = [[info objectForKey:UIImagePickerControllerOriginalImage] retain];
		callback->DidDismiss(pickedImage, info);
		[pickedImage release];
	}
	else
	{
		callback->DidDismiss(nil, nil);
	}
}

@end

@interface IPhonePHPickerControllerDelegate : NSObject< PHPickerViewControllerDelegate >

@property (nonatomic, readwrite, assign) Rtt::IPhoneImageProvider* callback;

@end

@implementation IPhonePHPickerControllerDelegate

@synthesize callback;

- (void)picker:(PHPickerViewController *)picker didFinishPicking:(NSArray<PHPickerResult *> *)results  API_AVAILABLE(ios(14)){
    [picker dismissViewControllerAnimated:YES completion:nil];
    
    if ( picker )
    {
        callback->DidDismissForMultipleSelection( results );
    }
    else
    {
        callback->DidDismiss(nil, nil);
    }
}
@end


// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

IPhoneImageProvider::IPhoneImageProvider( const ResourceHandle<lua_State> & handle )
:	PlatformImageProvider( handle ),
    fDstBaseName( nil ),
	fDstPath( nil ),
	iOS5statusBarHidden( false )
{
	fDelegate = [[IPhoneImagePickerControllerDelegate alloc] init];
	fDelegate.callback = this;
    fDelegateForMulti = [[IPhonePHPickerControllerDelegate alloc] init];
    fDelegateForMulti.callback = this;
	fMediaProvider = Rtt_NEW( LuaContext::GetAllocator( handle.Dereference() ), IPhoneMediaProvider );
}

IPhoneImageProvider::~IPhoneImageProvider()
{
//	Cleanup();
	[fDstBaseName release];
    [fDstPath release];
	[fDelegate release];
    [fDelegateForMulti release];
	Rtt_DELETE( fMediaProvider );
}

void
IPhoneImageProvider::Initialize()
{
}
	
bool
IPhoneImageProvider::Supports( int source ) const
{
	return [UIImagePickerController isSourceTypeAvailable:fMediaProvider->MediaProviderTypeToImagePickerSourceType( source )];
}

bool
IPhoneImageProvider::HasAccessTo( int source ) const
{
	bool sourceTypeAvailable = Supports(source);
	
	// We need to check the privacy settings if we're on a device/OS version that supports it.
	if (sourceTypeAvailable)
	{
		AuthorizationStatus authStatus = getAuthorizationStatusForSourceType( source );
		switch ( authStatus )
		{
			case AuthorizationStatusNotDetermined:
				// we assume that by default user would allow usage on iOS
				return true;
				break;
			case AuthorizationStatusAuthorized:
				// The source is present and we have access to it!
				return true;
				break;
			default:
				// Access is denied for one reason or another.
				return false;
				break;
		}
	}
	
	return sourceTypeAvailable;
}

bool
IPhoneImageProvider::Show( int source, const char* filePath, lua_State* L )
{
	[fDstPath release];
	if ( NULL != filePath )
	{
		fDstPath = [[NSString alloc] initWithUTF8String:filePath];
	}
	else
	{
		fDstPath = nil;
	}
	
	bool result = Rtt_VERIFY( Supports( source ) );

	if ( result )
	{
		/*
		If this is called from media.show then the ipad only parameters would be in the second table parameter
		eg. media.show( media.PhotoLibrary, { listener [, origin] [, permittedArrowDirections] } )
		If this is called from media.selectVideo then the table would be in the first parameter
		eg. media.selectVideo({ listener [, origin] [, permittedArrowDirections]})
		*/
		int tableIndex = 1;
		if ( lua_type( L, tableIndex ) != LUA_TTABLE )
		{
			tableIndex++;
		}

		fMediaProvider->Show( fMediaProvider->MediaProviderTypeToImagePickerSourceType( source ), (NSString *) kUTTypeImage, fDelegate, L, tableIndex, 0, (UIImagePickerControllerQualityType)0 );
	}
	else
	{
		EndSession();
	}

	return result;
}

bool
IPhoneImageProvider::ShowMulti( int source, PlatformImageProvider::ParametersForMultiSelection params, lua_State* L )
{
    bool result = false;
    
    if (@available(iOS 14, *))
    {
        [fDstPath release];
        if ( NULL != params.filePath )
        {
            fDstPath = [[NSString alloc] initWithUTF8String:params.filePath];
        }
        else
        {
            fDstPath = nil;
        }
        
        [fDstBaseName release];
        if ( NULL != params.fileName )
        {
            fDstBaseName = [[NSString alloc] initWithUTF8String:params.fileName];
        }
        else
        {
            fDstBaseName = nil;
        }

        bool result = Rtt_VERIFY( Supports( source ) );
        
        if ( result )
        {
            int tableIndex = 1;
            if ( lua_type( L, tableIndex ) != LUA_TTABLE )
            {
                tableIndex++;
            }
            
            fMediaProvider->ShowMulti( (NSString *) kUTTypeImage, fDelegateForMulti, L, tableIndex, params.maxSelection );
        }
        else
        {
            EndSession();
        }
    }
    else
    {
        result = Show( source, params.filePath, L );
    }

	return result;
}

void
IPhoneImageProvider::DidDismiss( UIImage* image, NSDictionary* editingInfo )
{
	AppleFileBitmap* bitmap = NULL;
	if ( image )
	{
		if ( fDstPath )
		{
			NSData *data = nil;
			NSString *lowercase = [fDstPath lowercaseString];
			if ( [lowercase hasSuffix:@"png"] )
			{
				data = UIImagePNGRepresentation( image );
			}
			else if ( [lowercase hasSuffix:@"jpg"] || [lowercase hasSuffix:@"jpeg"] )
			{
				data = UIImageJPEGRepresentation( image, 1.0 );
			}
			[data writeToFile:fDstPath atomically:YES];

			[fDstPath release];
			fDstPath = nil;
		}
		else
		{
			Rtt_Allocator* allocator = ((AppDelegate*)[[UIApplication sharedApplication] delegate]).runtime->Allocator(); Rtt_UNUSED( allocator );
			bitmap = Rtt_NEW( allocator, IPhoneFileBitmap( image ) );
		}
	}

	PlatformImageProvider::Parameters params( bitmap, NULL );
	params.wasCompleted = (image != NULL);
    if (bitmap == NULL)
    {
        params.multipleFilesCount = 1;
        Super::DidDismiss( AddPropertiesForMultiSelection, & params );
    }
    else
    {
        Super::DidDismiss( AddProperties, & params );
    }
	
	// iOS 5.0 introduces a bug where the status bar comes back if it is hidden on dismiss.
	// Seems to be fixed in 5.1 beta (unless iPod touch 4th gen was not originally affected)
	if ( fMediaProvider->Internal_IsOS5_0() && (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone) )
	{
		[[UIApplication sharedApplication] setStatusBarHidden:iOS5statusBarHidden withAnimation:UIStatusBarAnimationNone];
	}
}

API_AVAILABLE(ios(14))
void
IPhoneImageProvider::DidDismissForMultipleSelection( NSArray<PHPickerResult *> * results )
{
    __block int count = 0;
    if (fDstPath)
    {
        NSString *dstPathName = [fDstPath stringByDeletingPathExtension];
        NSString *extn = [fDstPath pathExtension];
        __block unsigned long index = 0;
        unsigned long numResults = [results count];
        for (PHPickerResult *result in results )
        {
            [result.itemProvider loadObjectOfClass:UIImage.self completionHandler:^(UIImage * image, NSError * _Nullable error) {
                index++;
                if (image) {
                    NSString *filePath = fDstPath;
                    if (count > 0)
                    {
                        NSString *suffix = [NSString stringWithFormat:@"_%d", count + 1];
                        filePath = [[dstPathName stringByAppendingString:suffix] stringByAppendingPathExtension:extn];
                    }
                    NSData *data = nil;
                    NSString *lowercase = [filePath lowercaseString];
                    if ( [lowercase hasSuffix:@"png"] )
                    {
                        data = UIImagePNGRepresentation( image );
                    }
                    else if ( [lowercase hasSuffix:@"jpg"] || [lowercase hasSuffix:@"jpeg"] )
                    {
                        data = UIImageJPEGRepresentation( image, 1.0 );
                    }
                    [data writeToFile:filePath atomically:YES];
                    count++;
                }
                if (index >= numResults)
                {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        PlatformImageProvider::Parameters params( NULL, NULL );
                        params.multipleFilesCount = count;
                        params.multipleFilesBaseName = [fDstBaseName UTF8String];
                        params.wasCompleted = true;
                        Super::DidDismiss( AddPropertiesForMultiSelection, & params );
                    });
                }
            }];
        }
    }
    else
    {
        PlatformImageProvider::Parameters params( NULL, NULL );
        Super::DidDismiss( AddPropertiesForMultiSelection, & params );
    }
}
	
/*
void
IPhoneImageProvider::SetProperty( U32 mask, bool newValue )
{
	Super::SetProperty( mask, newValue );

}
*/

AuthorizationStatus
IPhoneImageProvider::getAuthorizationStatusForSourceType( int source )
{
	switch ( source )
	{
		case PlatformMediaProviderBase::kCamera:
			if ( [AVCaptureDevice respondsToSelector:@selector(authorizationStatusForMediaType:)] )
			{
				// There's no privacy setting for the Camera prior to iOS 8.
				// Calling this on iOS 7 will always return AuthorizationStatusAuthorized!
				return (AuthorizationStatus)[AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
			}
			else return AuthorizationStatusAuthorized;
			break;
		case PlatformMediaProviderBase::kPhotoLibrary:
		case PlatformMediaProviderBase::kSavedPhotosAlbum:
			if ( [PHPhotoLibrary class] )
			{
				return (AuthorizationStatus)[PHPhotoLibrary authorizationStatus];
			}
			else
			{
				return (AuthorizationStatus)[ALAssetsLibrary authorizationStatus];
			}
			break;
		default:
			return AuthorizationStatusNotDetermined;
			break;
	}
	
	return AuthorizationStatusNotDetermined;
}


////TODO: move this code to native.showPopup("requestPermissions") and remove deadlock...
//bool
//IPhoneImageProvider::requestAuthorizationForSourceType( int source )
//{
//	// Request access to the source.
//	__block bool authorizationGranted = false;
//	dispatch_semaphore_t authorizationSemaphore = dispatch_semaphore_create(0);
//
//	switch ( source )
//	{
//		case PlatformMediaProviderBase::kCamera:
//			if ( [AVCaptureDevice respondsToSelector:@selector(requestAccessForMediaType:completionHandler:)] )
//			{
//				[AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
//					authorizationGranted = granted;
//					dispatch_semaphore_signal(authorizationSemaphore);
//				}];
//			} // There's no privacy setting for the Camera prior to iOS 8.
//			break;
//		case PlatformMediaProviderBase::kPhotoLibrary:
//		case PlatformMediaProviderBase::kSavedPhotosAlbum:
//			if ( [PHPhotoLibrary class] )
//			{
//				// We get access through the photo library.
//				[PHPhotoLibrary requestAuthorization:^(PHAuthorizationStatus status) {
//					authorizationGranted = PHAuthorizationStatusAuthorized == status;
//					dispatch_semaphore_signal(authorizationSemaphore);
//				}];
//			}
//			else
//			{
//				// We have to get access to the assets library.
//				ALAssetsLibrary *assetsLibrary = [[ALAssetsLibrary alloc] init];
//				
//				// Manually put this on background thread since the callbacks run on the main thread by default!
//				dispatch_async( dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0 ), ^{
//					[assetsLibrary enumerateGroupsWithTypes:ALAssetsGroupAll usingBlock:^(ALAssetsGroup *group, BOOL *stop) {
//						if (*stop) {
//							// User hit the OK button.
//							authorizationGranted = true;
//							dispatch_semaphore_signal(authorizationSemaphore);
//							return;
//						}
//						*stop = TRUE;
//					} failureBlock:^(NSError *error) {
//						// User hit the Don't Allow button.
//						authorizationGranted = false;
//						dispatch_semaphore_signal(authorizationSemaphore);
//					}];
//				});
//			}
//			break;
//		default:
//			return false;
//			break;
//	}
//	
//	// Wait for a result from the user before proceeding.
//	// Corona is suspended while the permission request
//	// dialog is up so this shouldn't be harmful to us.
//	dispatch_semaphore_wait(authorizationSemaphore, DISPATCH_TIME_FOREVER);
//	dispatch_release(authorizationSemaphore);
//	return authorizationGranted;
//}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

