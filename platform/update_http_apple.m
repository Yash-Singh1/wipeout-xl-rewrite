#import <Foundation/Foundation.h>
#include <SDL3/SDL.h>
#include "updates.h"

/* Runs on the release-check worker. System TLS and proxy settings stay in charge. */
int PortUpdateFetch(char *url, size_t size)
{
    @autoreleasepool {
        NSURLSessionConfiguration *config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        config.timeoutIntervalForRequest = 5;
        config.timeoutIntervalForResource = 8;
        NSURLSession *session = [NSURLSession sessionWithConfiguration:config];
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:
            [NSURL URLWithString:@PORT_RELEASES "/latest"]];
        request.HTTPMethod = @"HEAD";
        [request setValue:@"WipeoutXL release check" forHTTPHeaderField:@"User-Agent"];
        dispatch_semaphore_t finished = dispatch_semaphore_create(0);
        __block struct { char url[256]; } result = {0};
        NSURLSessionDataTask *task = [session dataTaskWithRequest:request completionHandler:
            ^(NSData *data, NSURLResponse *response, NSError *error) {
                if (!error && [response isKindOfClass:[NSHTTPURLResponse class]] &&
                    ((NSHTTPURLResponse *)response).statusCode == 200) {
                    const char *text = response.URL.absoluteString.UTF8String;
                    if (text && strlen(text) < sizeof(result.url)) SDL_strlcpy(result.url, text, sizeof(result.url));
                }
                dispatch_semaphore_signal(finished);
            }];
        [task resume];
        dispatch_semaphore_wait(finished, DISPATCH_TIME_FOREVER);
        [session finishTasksAndInvalidate];
        if (!result.url[0] || strlen(result.url) >= size) return 0;
        SDL_strlcpy(url, result.url, size);
        return 1;
    }
}
