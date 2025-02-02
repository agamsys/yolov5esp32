#ifndef ELOQUENT_ESP32CAM_EDGEIMPULSE_yolo_DAEMON_H
#define ELOQUENT_ESP32CAM_EDGEIMPULSE_yolo_DAEMON_H 1

#include <functional>
#include "../camera/camera.h"
#include "../extra/esp32/multiprocessing/thread.h"
#include "./bbox.h"

using eloq::camera;
using eloq::ei::bbox_t;
using Eloquent::Extra::Esp32::Multiprocessing::Thread;
using OnObjectCallback = std::function<void(uint8_t, bbox_t&)>;
using OnNothingCallback = std::function<void()>;


namespace Eloquent {
    namespace Esp32cam {
        namespace EdgeImpulse {
            /**
             * Run yolo in background
             * 
             * @class yoloDaemon
             * @author Simone
             * @date 13/12/2023
             * @file yolo_daemon.h
             * @brief 
             */
             template<typename T>
            class yoloDaemon {
            public:
                Thread thread;
                
                /**
                 * @brief Constructor
                 * @param yolo
                 */
                yoloDaemon(T *yolo) :
                    thread("yolo"),
                    _yolo(yolo),
                    _numListeners(0) {
                        
                    }
            
                /**
                 * @brief Run function when an object is detected
                 * @param callback
                 */
                bool whenYouSeeAny(OnObjectCallback callback) {
                    return whenYouSee("*", callback);
                }
                
                /**
                 * @brief Run function when nothing is detected
                 * @param callback
                 */
                void whenYouDontSeeAnything(OnNothingCallback callback) {
                    _onNothing = callback;
                }
                
                /**
                 * @brief Run function when a specific object is detected
                 * @param label
                 * @param callback
                 */
                bool whenYouSee(String label, OnObjectCallback callback) {
                    if (_numListeners >= EI_CLASSIFIER_LABEL_COUNT + 1) {
                        ESP_LOGE("yolo daemon", "Max number of listeners reached");
                        return false;
                    }
                    
                    _callbacks[_numListeners].label = label;
                    _callbacks[_numListeners].callback = callback;
                    _numListeners += 1;
                    
                    return true;
                }
                
                /**
                 * Start yolo in background
                 * 
                 * @brief 
                 */
                void start() {
                    thread
                        .withArgs((void*) this)
                        .withStackSize(4000)
                        // @see https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-guides/performance/speed.html
                        .withPriority(17)
                        .run([](void *args) {
                            yoloDaemon *self = (yoloDaemon*) args;
                            
                            delay(3000);
                            
                            while (true) {
                                yield();
                                delay(1);
                                
                                if (!camera.capture().isOk())
                                    continue;

                                if (!self->_yolo->run().isOk())
                                    continue;
                                    
                                if (!self->_yolo->foundAnyObject()) {
                                    if (self->_onNothing)
                                        self->_onNothing();
                                        
                                    continue;
                                }
                                    
                                self->_yolo->forEach([&self](int i, bbox_t& bbox) {
                                    // run specific label callback
                                    for (uint8_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT + 1; i++) {
                                        String label = self->_callbacks[i].label;
                                        
                                        if (label == "*" || label == bbox.label)
                                            self->_callbacks[i].callback(i, bbox);
                                    }
                                });
                            }
                        });
                }
                
            protected:
                T *_yolo;
                uint8_t _numListeners;
                OnNothingCallback _onNothing;
                struct {
                    String label;
                    OnObjectCallback callback;
                } _callbacks[EI_CLASSIFIER_LABEL_COUNT + 1];
                
            };
        }
    }
}


#endif