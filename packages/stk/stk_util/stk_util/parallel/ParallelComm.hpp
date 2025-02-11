// Copyright 2002 - 2008, 2010, 2011 National Technology Engineering
// Solutions of Sandia, LLC (NTESS). Under the terms of Contract
// DE-NA0003525 with NTESS, the U.S. Government retains certain rights
// in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
// 
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
// 
//     * Neither the name of NTESS nor the names of its contributors
//       may be used to endorse or promote products derived from this
//       software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 

#ifndef stk_util_parallel_ParallelComm_hpp
#define stk_util_parallel_ParallelComm_hpp

#include "stk_util/parallel/Parallel.hpp"   // for MPI_Irecv, MPI_Wait, MPI_Barrier, MPI_Send
#include "stk_util/stk_config.h"            // for STK_HAS_MPI
#include "stk_util/util/ReportHandler.hpp"  // for ThrowAssertMsg, ThrowRequire
#include <cstddef>                          // for size_t, ptrdiff_t
#include <map>                              // for map
#include <stdexcept>                        // for runtime_error
#include <string>                           // for string
#include <vector>                           // for vector

namespace stk {

/** Perform collective all-to-all communication with individually
 *  varying message sizes.  The collective operation uses an
 *  all-to-all if the maximum number of sends or receives from
 *  any one processor is greater than the given bounds.
 *
 *  This is a work-horse communication for mesh data structures
 *  with parallel domain decomposition.
 */
class CommSparse;
class CommNeighbors;
class CommBroadcast;

template<typename T>
struct is_pair { static constexpr bool value = false; };

template<template<typename...> class C, typename U, typename V>
struct is_pair<C<U,V>> { static constexpr bool value = std::is_same<C<U,V>, std::pair<U,V>>::value; };

template <typename T>
using IsPair = std::enable_if_t<is_pair<T>::value>;

template <typename T>
using NotPair = std::enable_if_t<!is_pair<T>::value>;

class CommBuffer {
public:

  /** Pack a value to be sent:  buf.pack<type>( value ) */
  template<typename T,
           class = NotPair<T>>
  CommBuffer &pack( const T & value );

  CommBuffer &pack( const std::string & value );

  template<typename P,
           class = IsPair<P>, class = void>
  CommBuffer &pack(const P & value);

  template<typename K, typename V>
  CommBuffer &pack( const std::map<K,V> & value );

  template<typename K>
  CommBuffer &pack( const std::vector<K> & value );

private:
  /** Do not try to pack a pointer for global communication */
  template<typename T> CommBuffer &pack( const T* value ) {
    ThrowAssertMsg(false,"CommBuffer::pack(const T* value) not allowed. Don't pack a pointer for communication!");
    return *this;
  }

public:

  /** Pack an array of values to be sent:  buf.pack<type>( ptr , num ) */
  template<typename T> CommBuffer &pack( const T * value , size_t number );

  /** Unpack a received value:  buf.unpack<type>( value ) */
  template<typename T,
           class = NotPair<T>>
  CommBuffer &unpack( T & value );

  CommBuffer &unpack( std::string& value );

  template<typename P,
           class = IsPair<P>, class = void>
  CommBuffer &unpack( P & value);

  template<typename K, typename V>
  CommBuffer &unpack( std::map<K,V> & value );

  template<typename K>
  CommBuffer &unpack( std::vector<K> & value );

  /** Unpack an array of received values:  buf.unpack<type>( ptr , num ) */
  template<typename T> CommBuffer &unpack( T * value , size_t number );

  /** Peek at a received value (don't advance buffer): buf.peek<type>(value) */
  template<typename T> CommBuffer &peek( T & value );

  /** Peek at an array of received values: buf.peek<type>( ptr , num ) */
  template<typename T> CommBuffer &peek( T * value , size_t number );

  CommBuffer &peek( std::string& value );

  template<typename K, typename V>
  CommBuffer &peek( std::map<K,V> & value );

  /** Skip buffer ahead by a number of values. */
  template<typename T,
           class = NotPair<T>>
  CommBuffer &skip( size_t number );

  /** Skip buffer ahead by a number of values. */
  template<typename T,
           class = IsPair<T>, class = void>
  CommBuffer &skip( size_t number );

  /** Reset the buffer to the beginning so that size() == 0 */
  void reset();

  /** Size, in bytes, of the buffer.
   *  If the buffer is not yet allocated this is zero.
   */
  size_t capacity() const ;

  // TODO - terribly misinforming when used on recv buffer, returns 0!
  /** Size, in bytes, of the buffer that has been processed.
   *  If the buffer is not yet allocated then this is the
   *  number of bytes that has been attempted to pack.
   */
  size_t size() const ;
  void set_size(size_t newsize_bytes);

  /** Size, in bytes, of the buffer remaining to be processed.
   *  Equal to 'capacity() - size()'.  A negative result
   *  indicates either the buffer is not allocated or an
   *  overflow has occurred.  An overflow will have thrown
   *  an exception.
   */
  ptrdiff_t remaining() const ;

  /** Pointer to base of buffer. */
  void * buffer() const ;

  ~CommBuffer() {}
  CommBuffer() : m_beg(nullptr), m_ptr(nullptr), m_end(nullptr) { }

  void set_buffer_ptrs(unsigned char* begin, unsigned char* ptr, unsigned char* end);

private:
  friend class CommSparse ;
  friend class CommNeighbors ;
  friend class CommBroadcast ;

  void pack_overflow() const ;
  void unpack_overflow() const ;

  typedef unsigned char * ucharp ;

  ucharp m_beg ;
  ucharp m_ptr ;
  ucharp m_end ;
};

//------------------------------------------------------------------------

class CommBroadcast {
public:

  ParallelMachine parallel()      const { return m_comm ; }
  int             parallel_size() const { return m_size ; }
  int             parallel_rank() const { return m_rank ; }

  /** Obtain the message buffer for the root_rank processor */
  CommBuffer & send_buffer();

  /** Obtain the message buffer for the local processor */
  CommBuffer & recv_buffer();

  //----------------------------------------

  CommBroadcast( ParallelMachine , int root_rank );

  void communicate();

  bool allocate_buffer( const bool local_flag = false );

  ~CommBroadcast();

private:

  CommBroadcast();
  CommBroadcast( const CommBroadcast & );
  CommBroadcast & operator = ( const CommBroadcast & );

  ParallelMachine m_comm ;
  int             m_size ;
  int             m_rank ;
  int             m_root_rank ;
  CommBuffer      m_buffer ;
};

//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Inlined template implementations for the CommBuffer

template<unsigned N>
struct CommBufferAlign {
  static size_t align( size_t i ) { i %= N ; return i ? ( N - i ) : 0 ; }
};

template<>
struct CommBufferAlign<1> {
  static size_t align( size_t ) { return 0 ; }
};

template<typename T, class>
inline
CommBuffer &CommBuffer::pack( const T & value )
{
  if (std::is_same<T, std::string>::value) {
    return pack(value);
  }
  enum { Size = sizeof(T) };
  size_t nalign = CommBufferAlign<Size>::align( m_ptr - m_beg );
  if ( m_beg ) {
    if ( m_end < m_ptr + nalign + Size ) { pack_overflow(); }
    while ( nalign ) { --nalign ; *m_ptr = 0 ; ++m_ptr ; }
    T * tmp = reinterpret_cast<T*>(m_ptr);
    *tmp = value ;
    m_ptr = reinterpret_cast<ucharp>( ++tmp );
  }
  else {
    m_ptr += nalign + Size ;
  }
  return *this;
}

inline
CommBuffer &CommBuffer::pack( const std::string & value )
{
  size_t length = value.length();
  pack(length);
  pack(value.c_str(), length);
  return *this;
}

template<typename P, class, class>
inline
CommBuffer &CommBuffer::pack(const P & value)
{
  pack(value.first);
  pack(value.second);
  return *this;
}

template<typename K, typename V>
inline
CommBuffer &CommBuffer::pack( const std::map<K,V> & value )
{
  size_t ns = value.size();
  pack(ns);

  for (auto && s : value)
  {
    pack(s.first);
    pack(s.second);
  }

  return *this;
}

template<typename K>
inline
CommBuffer &CommBuffer::pack( const std::vector<K> & value )
{
  pack<unsigned>(value.size());
  for (size_t i=0; i<value.size(); ++i) {
    pack(value[i]);
  }
  return *this;
}

template<typename T>
inline
CommBuffer &CommBuffer::pack( const T * value , size_t number )
{
  enum { Size = sizeof(T) };
  size_t nalign = CommBufferAlign<Size>::align( m_ptr - m_beg );
  if ( m_beg ) {
    if ( m_end < m_ptr + nalign + number * Size ) { pack_overflow(); }
    while ( nalign ) { --nalign ; *m_ptr = 0 ; ++m_ptr ; }
    T * tmp = reinterpret_cast<T*>(m_ptr);
    while ( number ) { --number ; *tmp = *value ; ++tmp ; ++value ; }
    m_ptr = reinterpret_cast<ucharp>( tmp );
  }
  else {
    m_ptr += nalign + number * Size ;
  }
  return *this;
}

template<typename T, class>
inline
CommBuffer &CommBuffer::skip( size_t number )
{
  enum { Size = sizeof(T) };
  m_ptr += CommBufferAlign<Size>::align( m_ptr - m_beg ) + Size * number ;
  if ( m_beg && m_end < m_ptr ) { unpack_overflow(); }
  return *this;
}

template<typename T, class, class>
inline
CommBuffer &CommBuffer::skip( size_t number )
{
  skip<typename T::first_type>(number);
  skip<typename T::second_type>(number);
  return *this;
}

template<typename T, class>
inline
CommBuffer &CommBuffer::unpack( T & value )
{
  if (std::is_same<T,std::string>::value) {
    return unpack(value);
  }
  enum { Size = sizeof(T) };
  const size_t nalign = CommBufferAlign<Size>::align( m_ptr - m_beg );
  T * tmp = reinterpret_cast<T*>( m_ptr + nalign );
  value = *tmp ;
  m_ptr = reinterpret_cast<ucharp>( ++tmp );
  if ( m_end < m_ptr ) { unpack_overflow(); }
  return *this;
}

inline
CommBuffer &CommBuffer::unpack( std::string & value )
{
  size_t length;
  unpack(length);
  std::vector<char> chars(length);
  unpack(chars.data(), length);
  value.assign(chars.data(), length);
  return *this;
}

template<typename P,
         class, class>
inline
CommBuffer &CommBuffer::unpack( P & value)
{
  unpack(value.first);
  unpack(value.second);
  return *this;
}

template<typename K, typename V>
inline
CommBuffer &CommBuffer::unpack( std::map<K,V> & value )
{
  value.clear();

  size_t ns;
  unpack(ns);

  for (size_t i = 0; i < ns; ++i)
  {
    K key;
    unpack(key);

    V val;
    unpack(val);

    value[key] = val;
  }
  return *this;
}

template<typename K>
inline
CommBuffer &CommBuffer::unpack( std::vector<K> & value )
{
  unsigned num_items = 0;
  unpack<unsigned>(num_items);
  value.resize(num_items);
  for (unsigned i=0;i<num_items;++i) {
    K val;
    unpack(val);
    value[i] = val;
  }
  return *this;
}

template<typename T>
inline
CommBuffer &CommBuffer::unpack( T * value , size_t number )
{
  enum { Size = sizeof(T) };
  const size_t nalign = CommBufferAlign<Size>::align( m_ptr - m_beg );
  T * tmp = reinterpret_cast<T*>( m_ptr + nalign );
  while ( number ) { --number ; *value = *tmp ; ++tmp ; ++value ; }
  m_ptr = reinterpret_cast<ucharp>( tmp );
  if ( m_end < m_ptr ) { unpack_overflow(); }
  return *this;
}
template<typename item>
inline
item unpack(stk::CommBuffer& buf)
{
    item object;
    buf.unpack<item>(object);
    return object;
}

template<typename T>
inline
CommBuffer &CommBuffer::peek( T & value )
{
  ucharp oldPtr = m_ptr;
  unpack<T>(value);
  m_ptr = oldPtr;
  return *this;
}

inline
CommBuffer &CommBuffer::peek( std::string& value )
{
  size_t length;
  peek(length);

  size_t offset = sizeof(size_t);
  std::vector<char> chars(offset+length);
  peek(chars.data(), chars.size());

  value.assign(&chars[offset], length);

  return *this;
}

template<typename K, typename V>
inline
CommBuffer &CommBuffer::peek( std::map<K,V> & value )
{
  throw std::runtime_error("Peek not implemented for std::map");
}

template<typename T>
inline
CommBuffer &CommBuffer::peek( T * value , size_t number )
{
  enum { Size = sizeof(T) };
  const size_t nalign = CommBufferAlign<Size>::align( m_ptr - m_beg );
  T * tmp = reinterpret_cast<T*>( m_ptr + nalign );
  while ( number ) { --number ; *value = *tmp ; ++tmp ; ++value ; }
  if ( m_end < reinterpret_cast<ucharp>(tmp) ) { unpack_overflow(); }
  return *this;
}

inline
void CommBuffer::reset()
{ m_ptr = m_beg ; }

inline
size_t CommBuffer::capacity() const
{ return m_end - m_beg ; }

inline
size_t CommBuffer::size() const
{ return m_ptr - m_beg ; }

inline
void CommBuffer::set_size(size_t newsize_bytes)
{ m_beg = nullptr;  m_ptr = nullptr; m_ptr += newsize_bytes ; m_end = nullptr; }

inline
ptrdiff_t CommBuffer::remaining() const
{ return m_end - m_ptr ; }

inline
void * CommBuffer::buffer() const
{ return static_cast<void*>( m_beg ); }

std::vector<int> ComputeReceiveList(std::vector<int>& sendSizeArray, MPI_Comm &mpi_communicator);

//
//  Parallel_Data_Exchange: General object exchange template with unknown comm plan
//
template<typename T>
void parallel_data_exchange_t(std::vector< std::vector<T> > &send_lists,
                              std::vector< std::vector<T> > &recv_lists,
                              MPI_Comm &mpi_communicator ) {
#ifdef STK_HAS_MPI
  //
  //  Determine the number of processors involved in this communication
  //
  const int msg_tag = 10242;
  int num_procs;
  MPI_Comm_size(mpi_communicator, &num_procs);
  int my_proc;
  MPI_Comm_rank(mpi_communicator, &my_proc);
  ThrowRequire((unsigned int) num_procs == send_lists.size() && (unsigned int) num_procs == recv_lists.size());
  int class_size = sizeof(T);
  //
  //  Determine number of items each other processor will send to the current processor
  //
  std::vector<int> global_number_to_send(num_procs);
  for(int iproc=0; iproc<num_procs; ++iproc) {
    global_number_to_send[iproc] = send_lists[iproc].size();
  }
  std::vector<int> numToRecvFrom = ComputeReceiveList(global_number_to_send, mpi_communicator);
  //
  //  Send the actual messages as raw byte streams.
  //
  std::vector<MPI_Request> recv_handles(num_procs);
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    recv_lists[iproc].resize(numToRecvFrom[iproc]);
    if(recv_lists[iproc].size() > 0) {
      char* recv_buffer = (char*)recv_lists[iproc].data();
      int recv_size = recv_lists[iproc].size()*class_size;
      MPI_Irecv(recv_buffer, recv_size, MPI_CHAR, iproc, msg_tag, mpi_communicator, &recv_handles[iproc]);
    }
  }
  MPI_Barrier(mpi_communicator);
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    if(send_lists[iproc].size() > 0) {
      char* send_buffer = (char*)send_lists[iproc].data();
      int send_size = send_lists[iproc].size()*class_size;
      MPI_Send(send_buffer, send_size, MPI_CHAR,
               iproc, msg_tag, mpi_communicator);
    }
  }
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    if(recv_lists[iproc].size() > 0) {
      MPI_Status status;
      MPI_Wait( &recv_handles[iproc], &status );
    }
  }
#endif
}

//
//  Generalized comm plans
//
//  This plan assumes the send and recv lists have identical sizes so no extra sizing communications are needed
//
template<typename T>
void parallel_data_exchange_sym_t(std::vector< std::vector<T> > &send_lists,
                                  std::vector< std::vector<T> > &recv_lists,
                                  const MPI_Comm &mpi_communicator )
{
  //
  //  Determine the number of processors involved in this communication
  //
#if defined( STK_HAS_MPI)
  const int msg_tag = 10242;
  int num_procs = stk::parallel_machine_size(mpi_communicator);
  int class_size = sizeof(T);

  //
  //  Send the actual messages as raw byte streams.
  //
  std::vector<MPI_Request> recv_handles(num_procs);
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    recv_lists[iproc].resize(send_lists[iproc].size());
    if(recv_lists[iproc].size() > 0) {
      char* recv_buffer = (char*)recv_lists[iproc].data();
      int recv_size = recv_lists[iproc].size()*class_size;
      MPI_Irecv(recv_buffer, recv_size, MPI_CHAR,
                iproc, msg_tag, mpi_communicator, &recv_handles[iproc]);
    }
  }
  MPI_Barrier(mpi_communicator);
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    if(send_lists[iproc].size() > 0) {
      char* send_buffer = (char*)send_lists[iproc].data();
      int send_size = send_lists[iproc].size()*class_size;
      MPI_Send(send_buffer, send_size, MPI_CHAR,
               iproc, msg_tag, mpi_communicator);
    }
  }
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    if(recv_lists[iproc].size() > 0) {
      MPI_Status status;
      MPI_Wait( &recv_handles[iproc], &status );
    }
  }
#endif
}

template<typename T>
inline
void parallel_data_exchange_nonsym_known_sizes_t(const int* sendOffsets,
                                                 T* sendData,
                                                 const int* recvOffsets,
                                                 T* recvData,
                                                 MPI_Comm mpi_communicator )
{
#if defined( STK_HAS_MPI)
  const int msg_tag = 10243; //arbitrary tag value, anything less than 32768 is legal
  const int num_procs = stk::parallel_machine_size(mpi_communicator);
  const int bytesPerScalar = sizeof(T);

  //
  //  Send the actual messages as raw byte streams.
  //
  std::vector<MPI_Request> recv_handles(num_procs);
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    const int recvSize = recvOffsets[iproc+1]-recvOffsets[iproc];
    if(recvSize > 0) {
      char* recvBuffer = (char*)(&recvData[recvOffsets[iproc]]);
      const int recvSizeBytes = recvSize*bytesPerScalar;
      MPI_Irecv(recvBuffer, recvSizeBytes, MPI_CHAR, iproc, msg_tag, mpi_communicator, &recv_handles[iproc]);
    }
  }

  MPI_Barrier(mpi_communicator);

  for(int iproc = 0; iproc < num_procs; ++iproc) {
    const int sendSize = sendOffsets[iproc+1]-sendOffsets[iproc];
    if(sendSize > 0) {
      char* sendBuffer = (char*)(&sendData[sendOffsets[iproc]]);
      const int sendSizeBytes = sendSize*bytesPerScalar;
      MPI_Send(sendBuffer, sendSizeBytes, MPI_CHAR, iproc, msg_tag, mpi_communicator);
    }
  }

  for(int iproc = 0; iproc < num_procs; ++iproc) {
    const int recvSize = recvOffsets[iproc+1]-recvOffsets[iproc];
    if(recvSize > 0) {
      MPI_Status status;
      MPI_Wait( &recv_handles[iproc], &status );
    }
  }
#endif
}

//
//  This plan assumes the send and recv lists are matched, but that the actual amount of data to send is unknown.
//  A processor knows which other processors it will be receiving data from, but does not know how much data.
//  Thus the comm plan is known from the inputs, but an additional message sizing call must be done.
//
template<typename T>
void parallel_data_exchange_sym_unknown_size_t(std::vector< std::vector<T> > &send_lists,
                                               std::vector< std::vector<T> > &recv_lists,
                                               MPI_Comm &mpi_communicator )
{
#if defined( STK_HAS_MPI)
  const int msg_tag = 10242;
  int num_procs = stk::parallel_machine_size(mpi_communicator);
  int class_size = sizeof(T);

  //
  //  Send the message sizes
  //
  std::vector<int> send_msg_sizes(num_procs);
  std::vector<int> recv_msg_sizes(num_procs);
  std::vector<MPI_Request> recv_handles(num_procs);

  for(int iproc = 0; iproc < num_procs; ++iproc) {
    send_msg_sizes[iproc] = send_lists[iproc].size();
  }    
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    if(recv_lists[iproc].size()>0) {
      MPI_Irecv(&recv_msg_sizes[iproc], 1, MPI_INT, iproc, msg_tag, mpi_communicator, &recv_handles[iproc]);
    }
  }
  MPI_Barrier(mpi_communicator);
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    if(send_lists[iproc].size()>0) {
      MPI_Send(&send_msg_sizes[iproc], 1, MPI_INT, iproc, msg_tag, mpi_communicator);
    }
  }
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    if(recv_lists[iproc].size() > 0) {
      MPI_Status status;
      MPI_Wait( &recv_handles[iproc], &status );
      recv_lists[iproc].resize(recv_msg_sizes[iproc]);
    }
  }
  //
  //  Send the actual messages as raw byte streams.
  //
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    if(recv_lists[iproc].size() > 0) {
      char* recv_buffer = (char*)recv_lists[iproc].data();
      int recv_size = recv_lists[iproc].size()*class_size;
      MPI_Irecv(recv_buffer, recv_size, MPI_CHAR,
                iproc, msg_tag, mpi_communicator, &recv_handles[iproc]);
    }
  }
  MPI_Barrier(mpi_communicator);
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    if(send_lists[iproc].size() > 0) {
      char* send_buffer = (char*)send_lists[iproc].data();
      int send_size = send_lists[iproc].size()*class_size;
      MPI_Send(send_buffer, send_size, MPI_CHAR,
               iproc, msg_tag, mpi_communicator);
    }
  }
  for(int iproc = 0; iproc < num_procs; ++iproc) {
    if(recv_lists[iproc].size() > 0) {
      MPI_Status status;
      MPI_Wait( &recv_handles[iproc], &status );
    }
  }
#endif
}

template<typename T, typename MsgPacker, typename MsgUnpacker>
void parallel_data_exchange_sym_pack_unpack(MPI_Comm mpi_communicator,
                                            const std::vector<int>& comm_procs,
                                            MsgPacker& pack_msg,
                                            MsgUnpacker& unpack_msg,
                                            bool deterministic)
{
#if defined( STK_HAS_MPI)
  const int msg_tag = 10242;
  int class_size = sizeof(T);

  int num_comm_procs = comm_procs.size();
  std::vector<std::vector<T> > send_data(num_comm_procs);
  std::vector<std::vector<T> > recv_data(num_comm_procs);
  std::vector<MPI_Request> send_requests(num_comm_procs);
  std::vector<MPI_Request> recv_requests(num_comm_procs);
  std::vector<MPI_Status> statuses(num_comm_procs);

  for(int i=0; i<num_comm_procs; ++i) {
    int iproc = comm_procs[i];
    pack_msg(iproc, send_data[i]);
    recv_data[i].resize(send_data[i].size());

    char* recv_buffer = (char*)recv_data[i].data();
    int buf_size = recv_data[i].size()*class_size;
    MPI_Irecv(recv_buffer, buf_size, MPI_CHAR, iproc, msg_tag, mpi_communicator, &recv_requests[i]);
    char* send_buffer = (char*)send_data[i].data();
    MPI_Isend(send_buffer, buf_size, MPI_CHAR, iproc, msg_tag, mpi_communicator, &send_requests[i]);
  }

  MPI_Status status;
  for(int i = 0; i < num_comm_procs; ++i) {
      int idx = i;
      if (deterministic) {
          MPI_Wait(&recv_requests[i], &status);
      }   
      else {
          MPI_Waitany(num_comm_procs, recv_requests.data(), &idx, &status);
      }   
      unpack_msg(comm_procs[idx], recv_data[idx]);
  }

  MPI_Waitall(num_comm_procs, send_requests.data(), statuses.data());
#endif
}

}

//----------------------------------------------------------------------
//----------------------------------------------------------------------

#endif

