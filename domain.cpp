/*
        Author: Marco Costalba (C) 2005-2007

Copyright: See COPYING file that comes with this distribution

                */
#include "domain.h"
#include "git.h"
#include <QApplication>
#include <QStatusBar>
#include <QTimer>

using namespace QGit;

// ************************* Domain ****************************

Domain::Domain(QSharedPointer<Git> git, bool)
   : QObject()
   , mGit(git)
{
   st.clear();
   busy = linked = false;
}

Domain::~Domain()
{
   if (!parent())
      return;
}

void Domain::clear(bool complete)
{
   if (complete)
      st.clear();
}

void Domain::deleteWhenDone()
{
   emit cancelDomainProcesses();

   on_deleteWhenDone();
}

void Domain::on_deleteWhenDone()
{
   deleteLater();
}

void Domain::unlinkDomain(Domain *d)
{

   d->linked = false;
   while (d->disconnect(SIGNAL(updateRequested(StateInfo)), this))
      ; // a signal is emitted for every connection you make,
        // so if you duplicate a connection, two signals will be emitted.
}

void Domain::linkDomain(Domain *d)
{

   unlinkDomain(d); // be sure only one connection is active
   connect(d, &Domain::updateRequested, this, &Domain::on_updateRequested);
   d->linked = true;
}

void Domain::on_updateRequested(StateInfo newSt)
{

   st = newSt;
   update(false, false);
}

bool Domain::flushQueue()
{
   // during dragging any state update is queued, so try to flush pending now

   if (!busy && st.flushQueue())
   {
      update(false, false);
      return true;
   }
   return false;
}

void Domain::populateState()
{
   const auto r = mGit->revLookup(st.sha());

   if (r)
      st.setIsMerge(r->parentsCount() > 1);
}

void Domain::update(bool fromMaster, bool force)
{

   if (busy && st.requestPending())
   {
      emit cancelDomainProcesses();
   }
   if (busy)
      return;

   if (linked && !fromMaster)
   {
      // in this case let the update to fall down from master domain
      StateInfo tmp(st);
      st.rollBack(); // we don't want to filter out next update sent from master
      emit updateRequested(tmp);
      return;
   }

   mGit->setCurContext(this);
   busy = true;
   populateState(); // complete any missing state information
   st.setLock(true); // any state change will be queued now

   if (doUpdate(force))
      st.commit();
   else
      st.rollBack();

   st.setLock(false);
   busy = false;

   mGit->setCurContext(nullptr);

   flushQueue();
}
