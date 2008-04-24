/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "packet.h"
#include "ns3/assert.h"

namespace ns3 {

uint32_t Packet::m_globalUid = 0;

TypeId 
TagIterator::Item::GetTypeId (void) const
{
  return m_tid;
}
uint32_t 
TagIterator::Item::GetStart (void) const
{
  return m_start;
}
uint32_t 
TagIterator::Item::GetEnd (void) const
{
  return m_end;
}
void 
TagIterator::Item::GetTag (Mtag &tag) const
{
  if (tag.GetInstanceTypeId () != GetTypeId ())
    {
      NS_FATAL_ERROR ("The tag you provided is not of the right type.");
    }
  tag.Deserialize (m_buffer);
}
TagIterator::Item::Item (TypeId tid, uint32_t start, uint32_t end, MtagBuffer buffer)
  : m_tid (tid),
    m_start (start),
    m_end (end),
    m_buffer (buffer)
{}
bool 
TagIterator::HasNext (void) const
{
  return m_current.HasNext ();
}
TagIterator::Item 
TagIterator::Next (void)
{
  MtagList::Iterator::Item i = m_current.Next ();
  
  TagIterator::Item item = TagIterator::Item (i.tid, 
                                              i.start-m_current.GetOffsetStart (), 
                                              i.end-m_current.GetOffsetStart (), 
                                              i.buf);
  
  
  return item;
}
TagIterator::TagIterator (MtagList::Iterator i)
  : m_current (i)
{}


void 
Packet::Ref (void) const
{
  m_refCount++;
}
void 
Packet::Unref (void) const
{
  m_refCount--;
  if (m_refCount == 0)
    {
      delete this;
    }
}

Ptr<Packet> 
Packet::Copy (void) const
{
  // we need to invoke the copy constructor directly
  // rather than calling Create because the copy constructor
  // is private.
  return Ptr<Packet> (new Packet (*this), false);
}

Packet::Packet ()
  : m_buffer (),
    m_tags (),
    m_tagList (),
    m_metadata (m_globalUid, 0),
    m_refCount (1)
{
  m_globalUid++;
}

Packet::Packet (const Packet &o)
  : m_buffer (o.m_buffer),
    m_tags (o.m_tags),
    m_tagList (o.m_tagList),
    m_metadata (o.m_metadata),
    m_refCount (1)
{}

Packet &
Packet::operator = (const Packet &o)
{
  if (this == &o)
    {
      return *this;
    }
  m_buffer = o.m_buffer;
  m_tags = o.m_tags;
  m_tagList = o.m_tagList;
  m_metadata = o.m_metadata;
  return *this;
}

Packet::Packet (uint32_t size)
  : m_buffer (size),
    m_tags (),
    m_tagList (),
    m_metadata (m_globalUid, size),
    m_refCount (1)
{
  m_globalUid++;
}
Packet::Packet (uint8_t const*buffer, uint32_t size)
  : m_buffer (),
    m_tags (),
    m_tagList (),
    m_metadata (m_globalUid, size),
    m_refCount (1)
{
  m_globalUid++;
  m_buffer.AddAtStart (size);
  Buffer::Iterator i = m_buffer.Begin ();
  i.Write (buffer, size);
}

Packet::Packet (const Buffer &buffer, const Tags &tags, const MtagList &tagList, const PacketMetadata &metadata)
  : m_buffer (buffer),
    m_tags (tags),
    m_tagList (tagList),
    m_metadata (metadata),
    m_refCount (1)
{}

Ptr<Packet>
Packet::CreateFragment (uint32_t start, uint32_t length) const
{
  Buffer buffer = m_buffer.CreateFragment (start, length);
  NS_ASSERT (m_buffer.GetSize () >= start + length);
  uint32_t end = m_buffer.GetSize () - (start + length);
  PacketMetadata metadata = m_metadata.CreateFragment (start, end);
  // again, call the constructor directly rather than
  // through Create because it is private.
  return Ptr<Packet> (new Packet (buffer, m_tags, m_tagList, metadata), false);
}

uint32_t 
Packet::GetSize (void) const
{
  return m_buffer.GetSize ();
}

void
Packet::AddHeader (const Header &header)
{
  uint32_t size = header.GetSerializedSize ();
  uint32_t orgStart = m_buffer.GetCurrentStartOffset ();
  bool resized = m_buffer.AddAtStart (size);
  if (resized)
    {
      m_tagList.AddAtStart (m_buffer.GetCurrentStartOffset () - orgStart,
                            m_buffer.GetCurrentStartOffset () + size);
    }
  header.Serialize (m_buffer.Begin ());
  m_metadata.AddHeader (header, size);
}
uint32_t
Packet::RemoveHeader (Header &header)
{
  uint32_t deserialized = header.Deserialize (m_buffer.Begin ());
  m_buffer.RemoveAtStart (deserialized);
  m_metadata.RemoveHeader (header, deserialized);
  return deserialized;
}
void
Packet::AddTrailer (const Trailer &trailer)
{
  uint32_t size = trailer.GetSerializedSize ();
  uint32_t orgEnd = m_buffer.GetCurrentEndOffset ();
  bool resized = m_buffer.AddAtEnd (size);
  if (resized)
    {
      m_tagList.AddAtEnd (m_buffer.GetCurrentEndOffset () - orgEnd,
                          m_buffer.GetCurrentEndOffset () - size);
    }
  Buffer::Iterator end = m_buffer.End ();
  trailer.Serialize (end);
  m_metadata.AddTrailer (trailer, size);
}
uint32_t
Packet::RemoveTrailer (Trailer &trailer)
{
  uint32_t deserialized = trailer.Deserialize (m_buffer.End ());
  m_buffer.RemoveAtEnd (deserialized);
  m_metadata.RemoveTrailer (trailer, deserialized);
  return deserialized;
}

void 
Packet::AddAtEnd (Ptr<const Packet> packet)
{
  uint32_t aStart = m_buffer.GetCurrentStartOffset ();
  uint32_t bEnd = packet->m_buffer.GetCurrentEndOffset ();
  m_buffer.AddAtEnd (packet->m_buffer);
  uint32_t appendPrependOffset = m_buffer.GetCurrentEndOffset () - packet->m_buffer.GetSize ();
  m_tagList.AddAtEnd (m_buffer.GetCurrentStartOffset () - aStart, 
                      appendPrependOffset);
  MtagList copy = packet->m_tagList;
  copy.AddAtStart (m_buffer.GetCurrentEndOffset () - bEnd,
                   appendPrependOffset);
  m_tagList.Add (copy);
  m_metadata.AddAtEnd (packet->m_metadata);
}
void
Packet::AddPaddingAtEnd (uint32_t size)
{
  uint32_t orgEnd = m_buffer.GetCurrentEndOffset ();
  bool resized = m_buffer.AddAtEnd (size);
  if (resized)
    {
      m_tagList.AddAtEnd (m_buffer.GetCurrentEndOffset () - orgEnd,
                          m_buffer.GetCurrentEndOffset () - size);
    }
  m_metadata.AddPaddingAtEnd (size);
}
void 
Packet::RemoveAtEnd (uint32_t size)
{
  m_buffer.RemoveAtEnd (size);
  m_metadata.RemoveAtEnd (size);
}
void 
Packet::RemoveAtStart (uint32_t size)
{
  m_buffer.RemoveAtStart (size);
  m_metadata.RemoveAtStart (size);
}

void 
Packet::RemoveAllTags (void)
{
  m_tags.RemoveAll ();
  m_tagList.RemoveAll ();
}

uint8_t const *
Packet::PeekData (void) const
{
  return m_buffer.PeekData ();
}

uint32_t 
Packet::GetUid (void) const
{
  return m_metadata.GetUid ();
}

void 
Packet::PrintTags (std::ostream &os) const
{
  m_tags.Print (os, " ");
  // XXX: tagList.
}

void 
Packet::Print (std::ostream &os) const
{
  PacketMetadata::ItemIterator i = m_metadata.BeginItem (m_buffer);
  while (i.HasNext ())
    {
      PacketMetadata::Item item = i.Next ();
      if (item.isFragment)
        {
          switch (item.type) {
          case PacketMetadata::Item::PAYLOAD:
            os << "Payload";
            break;
          case PacketMetadata::Item::HEADER:
          case PacketMetadata::Item::TRAILER:
            os << item.tid.GetName ();
            break;
          }
          os << " Fragment [" << item.currentTrimedFromStart<<":"
             << (item.currentTrimedFromStart + item.currentSize) << "]";
        }
      else
        {
          switch (item.type) {
          case PacketMetadata::Item::PAYLOAD:
            os << "Payload (size=" << item.currentSize << ")";
            break;
          case PacketMetadata::Item::HEADER:
          case PacketMetadata::Item::TRAILER:
            os << item.tid.GetName () << " (";
            {
              NS_ASSERT (item.tid.HasConstructor ());
              Callback<ObjectBase *> constructor = item.tid.GetConstructor ();
              NS_ASSERT (!constructor.IsNull ());
              ObjectBase *instance = constructor ();
              NS_ASSERT (instance != 0);
              Chunk *chunk = dynamic_cast<Chunk *> (instance);
              NS_ASSERT (chunk != 0);
              chunk->Deserialize (item.current);
              chunk->Print (os);
              delete chunk;
            }
            os << ")";
            break;
          }          
        }
      if (i.HasNext ())
        {
          os << " ";
        }
    }
#if 0
  // The code below will work only if headers and trailers
  // define the right attributes which is not the case for
  // now. So, as a temporary measure, we use the 
  // headers' and trailers' Print method as shown above.
  PacketMetadata::ItemIterator i = m_metadata.BeginItem (m_buffer);
  while (i.HasNext ())
    {
      PacketMetadata::Item item = i.Next ();
      if (item.isFragment)
        {
          switch (item.type) {
          case PacketMetadata::Item::PAYLOAD:
            os << "Payload";
            break;
          case PacketMetadata::Item::HEADER:
          case PacketMetadata::Item::TRAILER:
            os << item.tid.GetName ();
            break;
          }
          os << " Fragment [" << item.currentTrimedFromStart<<":"
             << (item.currentTrimedFromStart + item.currentSize) << "]";
        }
      else
        {
          switch (item.type) {
          case PacketMetadata::Item::PAYLOAD:
            os << "Payload (size=" << item.currentSize << ")";
            break;
          case PacketMetadata::Item::HEADER:
          case PacketMetadata::Item::TRAILER:
            os << item.tid.GetName () << "(";
            {
              NS_ASSERT (item.tid.HasConstructor ());
              Callback<ObjectBase *> constructor = item.tid.GetConstructor ();
              NS_ASSERT (constructor.IsNull ());
              ObjectBase *instance = constructor ();
              NS_ASSERT (instance != 0);
              Chunk *chunk = dynamic_cast<Chunk *> (instance);
              NS_ASSERT (chunk != 0);
              chunk->Deserialize (item.current);
              for (uint32_t j = 0; j < item.tid.GetAttributeN (); j++)
                {
                  std::string attrName = item.tid.GetAttributeName (j);
                  std::string value;
                  bool ok = chunk->GetAttribute (attrName, value);
                  NS_ASSERT (ok);
                  os << attrName << "=" << value;
                  if ((j + 1) < item.tid.GetAttributeN ())
                    {
                      os << ",";
                    }
                }
            }
            os << ")";
            break;
          }          
        }
      if (i.HasNext ())
        {
          os << " ";
        }
    }
#endif   
}

PacketMetadata::ItemIterator 
Packet::BeginItem (void) const
{
  return m_metadata.BeginItem (m_buffer);
}

void
Packet::EnableMetadata (void)
{
  PacketMetadata::Enable ();
}

Buffer 
Packet::Serialize (void) const
{
  Buffer buffer;
  uint32_t reserve;

  // write metadata
  reserve = m_metadata.GetSerializedSize ();
  buffer.AddAtStart (reserve);
  m_metadata.Serialize (buffer.Begin (), reserve);

  // write tags
  reserve = m_tags.GetSerializedSize ();
  buffer.AddAtStart (reserve);
  m_tags.Serialize (buffer.Begin (), reserve);
  
  // aggregate byte buffer, metadata, and tags
  Buffer tmp = m_buffer.CreateFullCopy ();
  buffer.AddAtStart (tmp.GetSize ());
  buffer.Begin ().Write (tmp.Begin (), tmp.End ());
  
  // write byte buffer size.
  buffer.AddAtStart (4);
  buffer.Begin ().WriteU32 (m_buffer.GetSize ());

  return buffer;
}
void 
Packet::Deserialize (Buffer buffer)
{
  Buffer buf = buffer;
  // read size
  uint32_t packetSize = buf.Begin ().ReadU32 ();
  buf.RemoveAtStart (4);

  // read buffer.
  buf.RemoveAtEnd (buf.GetSize () - packetSize);
  m_buffer = buf;

  // read tags
  buffer.RemoveAtStart (4 + packetSize);
  uint32_t tagsDeserialized = m_tags.Deserialize (buffer.Begin ());
  buffer.RemoveAtStart (tagsDeserialized);

  // read metadata
  uint32_t metadataDeserialized = 
    m_metadata.Deserialize (buffer.Begin ());
  buffer.RemoveAtStart (metadataDeserialized);
}

void 
Packet::AddMtag (const Mtag &tag) const
{
  MtagList *list = const_cast<MtagList *> (&m_tagList);
  MtagBuffer buffer = list->Add (tag.GetInstanceTypeId (), tag.GetSerializedSize (), 
                                 m_buffer.GetCurrentStartOffset (),
                                 m_buffer.GetCurrentEndOffset ());
  tag.Serialize (buffer);
}
TagIterator 
Packet::GetTagIterator (void) const
{
  return TagIterator (m_tagList.Begin (m_buffer.GetCurrentStartOffset (), m_buffer.GetCurrentEndOffset ()));
}

bool 
Packet::FindFirstMatchingTag (Mtag &tag) const
{
  TypeId tid = tag.GetInstanceTypeId ();
  TagIterator i = GetTagIterator ();
  while (i.HasNext ())
    {
      TagIterator::Item item = i.Next ();
      if (tid == item.GetTypeId ())
        {
          item.GetTag (tag);
          return true;
        }
    }
  return false;
}

std::ostream& operator<< (std::ostream& os, const Packet &packet)
{
  packet.Print (os);
  return os;
}


} // namespace ns3



#ifdef RUN_SELF_TESTS

#include "ns3/test.h"
#include <string>
#include <stdarg.h>

using namespace ns3;

namespace {

class ATestTagBase : public Mtag
{
public:
  ATestTagBase () : m_error (false) {}
  bool m_error;
};

template <int N>
class ATestTag : public ATestTagBase
{
public:
  static TypeId GetTypeId (void) {
    std::ostringstream oss;
    oss << "anon::ATestTag<" << N << ">";
    static TypeId tid = TypeId (oss.str ().c_str ())
      .SetParent<Mtag> ()
      .AddConstructor<ATestTag<N> > ()
      .HideFromDocumentation ()
      ;
    return tid;
  }
  virtual TypeId GetInstanceTypeId (void) const {
    return GetTypeId ();
  }
  virtual uint32_t GetSerializedSize (void) const {
    return N;
  }
  virtual void Serialize (MtagBuffer buf) const {
    for (uint32_t i = 0; i < N; ++i)
      {
        buf.WriteU8 (N);
      }
  }
  virtual void Deserialize (MtagBuffer buf) {
    for (uint32_t i = 0; i < N; ++i)
      {
        uint8_t v = buf.ReadU8 ();
        if (v != N)
          {
            m_error = true;
          }
      }
  }
  ATestTag ()
    : ATestTagBase () {}
};

struct Expected
{
  Expected (uint32_t n_, uint32_t start_, uint32_t end_)
    : n (n_), start (start_), end (end_) {}
  
  uint32_t n;
  uint32_t start;
  uint32_t end;
};

}

#define E(a,b,c) a,b,c

#define CHECK(p, n, ...)                                \
  if (!DoCheck (p, __FILE__, __LINE__, n, __VA_ARGS__)) \
    {                                                   \
      result = false;                                   \
    }

namespace ns3 {


class PacketTest: public Test 
{
public:
  PacketTest ();
  virtual bool RunTests (void);
private:
  bool DoCheck (Ptr<const Packet> p, const char *file, int line, uint32_t n, ...);
};


PacketTest::PacketTest ()
  : Test ("Packet") {}

bool
PacketTest::DoCheck (Ptr<const Packet> p, const char *file, int line, uint32_t n, ...)
{
  bool result = true;
  std::vector<struct Expected> expected;
  va_list ap;
  va_start (ap, n);
  for (uint32_t k = 0; k < n; ++k)
    {
      uint32_t N = va_arg (ap, uint32_t);
      uint32_t start = va_arg (ap, uint32_t);
      uint32_t end = va_arg (ap, uint32_t);
      expected.push_back (Expected (N, start, end));
    }
  va_end (ap);

  TagIterator i = p->GetTagIterator ();
  uint32_t j = 0;
  while (i.HasNext () && j < expected.size ())
    {
      TagIterator::Item item = i.Next ();
      struct Expected e = expected[j];
      std::ostringstream oss;
      oss << "anon::ATestTag<" << e.n << ">";
      NS_TEST_ASSERT_EQUAL_FILELINE (item.GetTypeId ().GetName (), oss.str (), file, line);
      NS_TEST_ASSERT_EQUAL_FILELINE (item.GetStart (), e.start, file, line);
      NS_TEST_ASSERT_EQUAL_FILELINE (item.GetEnd (), e.end, file, line);
      ATestTagBase *tag = dynamic_cast<ATestTagBase *> (item.GetTypeId ().GetConstructor () ());
      NS_TEST_ASSERT (tag != 0);
      item.GetTag (*tag);
      NS_TEST_ASSERT (!tag->m_error);
      delete tag;
      j++;
    }
  return result;
}

bool
PacketTest::RunTests (void)
{
  bool result = true;

  Ptr<Packet> pkt1 = Create<Packet> (reinterpret_cast<const uint8_t*> ("hello"), 5);
  Ptr<Packet> pkt2 = Create<Packet> (reinterpret_cast<const uint8_t*> (" world"), 6);
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddAtEnd (pkt1);
  packet->AddAtEnd (pkt2);
  
  NS_TEST_ASSERT_EQUAL (packet->GetSize (), 11);

  std::string msg = std::string (reinterpret_cast<const char *>(packet->PeekData ()),
                                 packet->GetSize ());
  NS_TEST_ASSERT_EQUAL (msg, "hello world");


  Ptr<const Packet> p = Create<Packet> (1000);

  p->AddMtag (ATestTag<1> ());
  CHECK (p, 1, E (1, 0, 1000));
  Ptr<const Packet> copy = p->Copy ();
  CHECK (copy, 1, E (1, 0, 1000));

  p->AddMtag (ATestTag<2> ());
  CHECK (p, 2, E (1, 0, 1000), E(2, 0, 1000));
  CHECK (copy, 1, E (1, 0, 1000));

  Ptr<Packet> frag0 = p->CreateFragment (0, 10);
  Ptr<Packet> frag1 = p->CreateFragment (10, 90);
  Ptr<const Packet> frag2 = p->CreateFragment (100, 900);
  frag0->AddMtag (ATestTag<3> ());
  CHECK (frag0, 3, E (1, 0, 10), E(2, 0, 10), E (3, 0, 10));
  frag1->AddMtag (ATestTag<4> ());
  CHECK (frag1, 3, E (1, 0, 90), E(2, 0, 90), E (4, 0, 90));
  frag2->AddMtag (ATestTag<5> ());
  CHECK (frag2, 3, E (1, 0, 900), E(2, 0, 900), E (5, 0, 900));

  frag1->AddAtEnd (frag2);
  CHECK (frag1, 6, E (1, 0, 90), E(2, 0, 90), E (4, 0, 90), E (1, 90, 990), E(2, 90, 990), E (5, 90, 990));

  CHECK (frag0, 3, E (1, 0, 10), E(2, 0, 10), E (3, 0, 10));
  frag0->AddAtEnd (frag1);
  CHECK (frag0, 9, 
         E (1, 0, 10), E(2, 0, 10), E (3, 0, 10),
         E (1, 10, 100), E(2, 10, 100), E (4, 10, 100), 
         E (1, 100, 1000), E(2, 100, 1000), E (5, 100, 1000));

  return result;
}


static PacketTest g_packetTest;

}; // namespace ns3

#endif /* RUN_SELF_TESTS */
