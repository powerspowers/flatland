/*
  Copyright 1996-2003
  Simon Whiteside

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  $Id: skElementObjectEnumerator.h,v 1.11 2003/04/19 13:22:23 simkin_cvs Exp $
*/
#ifndef ELEMENTOBJECTENUMERATOR_H
#define ELEMENTOBJECTENUMERATOR_H

#include "skExecutable.h"
#include "skExecutableIterator.h"
#include "skElement.h"

class CLASSEXPORT skElementObject;

/**
 * This class enumerates the element children of an skElementObject
 */
class CLASSEXPORT skElementObjectEnumerator : public skExecutable, public skExecutableIterator{
 public:
  /** This contructs an skElementObject enumerator over all the children
   * @param element - the element to enumerate the children of
   * @param location - the owning XML element name
   */
  IMPORT_C skElementObjectEnumerator(skElementObject * element,const skString& location);
  /** This contructs an skElementObject enumerator over children with a particular tag name
   * @param element - the element to enumerate the children of
   * @param location - the owning XML element name
   * @param tag - the tag name to look for
   */
  IMPORT_C skElementObjectEnumerator(skElementObject * element,const skString& location,const skString& tag);
  /**
   * Destructor
   */
  IMPORT_C virtual ~skElementObjectEnumerator();
  /**
   * This method exposes the following methods to Simkin scripts:
   * "next" - returns the next element in the enumeration - or null if there are no more
   * "reset" - resets the iterator
   * @param s method name
   * @param args arguments to the function
   * @param r return value
   * @param ctxt context object to receive errors
   * @exception Symbian - a leaving function
   * @exception skParseException - if a syntax error is encountered while the script is running
   * @exception skRuntimeException - if an error occurs while the script is running
   */
  IMPORT_C bool method(const skString& s,skRValueArray& args,skRValue& r,skExecutableContext& ctxt);
  /**
   * This method implements the method in skExecutableIterator
   * @exception Symbian - a leaving function
   */
  IMPORT_C bool next(skRValue&);
 private:
  void findNextNode();

  skElementObject * m_Object;
  skString m_Location;
  int m_NodeNum;
  skString m_Tag;
};
#endif
