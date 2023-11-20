/*******************************************************************************
  MPLAB Harmony Application Header File

  Company:
    Microchip Technology Inc.

  File Name:
    app.h

  Summary:
    This header file provides prototypes and definitions for the application.

  Description:
    This header file provides function prototypes and data type definitions for
    the application.  Some of these are required by the system (such as the
    "APP_Initialize" and "APP_Tasks" prototypes) and some of them are only used
    internally by the application (such as the "APP_STATES" definition).  Both
    are defined here for convenience.
*******************************************************************************/

#ifndef _APP_H
#define _APP_H

#ifdef __cplusplus  // C++ Compatibility
extern "C" {
#endif

#define APP_VERSION "0.0.1"

void APP_Initialize ( void );

void APP_Tasks( void );

__attribute__((format(printf, 1, 2))) _Noreturn
void APP_panic(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* _APP_H */
