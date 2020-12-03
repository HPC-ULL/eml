/*!
 * Copyright (c) 2020 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * \author Alberto Cabrera Perez <Alberto.Cabrera@ull.es>
 * \date   nov-2020
 * \brief  Java Interface for ReliAble beNChmarking on androID platforms
 */
#include "es_ull_pcg_hpc_benchmark_meters_EMLMeter.h"
#include "eml.h"

JNIEXPORT jint JNICALL Java_es_ull_pcg_hpc_benchmark_meters_EMLMeter_EMLInit
        (JNIEnv * env, jclass clazz)
{
    int toReturn = emlInit();
    return toReturn;
}

/*
 * Class:     es_ull_pcg_hpc_benchmark_meters_EMLMeter
 * Method:    EMLShutdown
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_es_ull_pcg_hpc_benchmark_meters_EMLMeter_EMLShutdown
        (JNIEnv * env, jclass clazz)
{
    int toReturn = emlShutdown();
    return toReturn;
}

/*
 * Class:     es_ull_pcg_hpc_benchmark_meters_EMLMeter
 * Method:    start
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_es_ull_pcg_hpc_benchmark_meters_EMLMeter_start
(JNIEnv * env, jobject this_obj)
{
    emlStart();
}

/*
 * Class:     es_ull_pcg_hpc_benchmark_meters_EMLMeter
 * Method:    stop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_es_ull_pcg_hpc_benchmark_meters_EMLMeter_stop
    (JNIEnv * env, jobject this_obj)
{
    // Get the energy data first, to reduce overhead from JNI Calls
    size_t count;
    emlDeviceGetCount(&count);
    emlData_t* data[count];
    jdouble consumed;

    emlStop(data);

    jdouble total = 0.;
    for (size_t i = 0; i < count; i++) {
        emlDataGetConsumed(data[i], &consumed);
        total += consumed;
    }

    for (size_t i = 0; i < count; i++) {
        emlDataFree(data[i]);
    }

    jclass java_class = (*env)->GetObjectClass(env, this_obj);
    jfieldID id_energy = (*env)->GetFieldID(env, java_class, "energy", "D");
    
    (*env)->SetDoubleField(env, this_obj, id_energy, total);
}

/*
 * Class:     es_ull_pcg_hpc_benchmark_meters_EMLMeter
 * Method:    stopError
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_es_ull_pcg_hpc_benchmark_meters_EMLMeter_stopError
    (JNIEnv * env, jobject this_obj)
{
    // Get the energy data first, to reduce overhead from JNI Calls
    size_t count;
    emlDeviceGetCount(&count);
    emlData_t* data[count];
    jdouble consumed;

    emlStop(data);
    for (size_t i = 0; i < count; i++) {
        emlDataFree(data[i]);
    }
}

/*
 * Class:     es_ull_pcg_hpc_benchmark_meters_EMLMeter
 * Method:    reset
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_es_ull_pcg_hpc_benchmark_meters_EMLMeter_reset
    (JNIEnv * env, jobject this_obj)
{

}
