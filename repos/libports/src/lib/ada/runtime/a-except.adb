--
--  \brief  Ada exceptions
--  \author Johannes Kliemann
--  \date   2018-06-25
--
--  Copyright (C) 2018 Genode Labs GmbH
--  Copyright (C) 2018 Componolit GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

package body Ada.Exceptions is

   ----------------------------
   -- Raise_Exception_Always --
   ----------------------------

   procedure Raise_Exception_Always (
                                     E       : Exception_Id;
                                     Message : String := ""
                                    )
   is
      procedure Raise_Ada_Exception (
                                     Name : System.Address;
                                     Msg  : System.Address
                                    )
        with
          Import,
          Convention => C,
          External_Name => "raise_ada_exception";
      C_Msg : String := Message & Character'Val (0);
   begin
      Warn_Not_Implemented ("Raise_Exception_Always");
      Raise_Ada_Exception (E.Full_Name, C_Msg'Address);
   end Raise_Exception_Always;

   procedure Raise_Exception (
                              E       : Exception_Id;
                              Message : String := ""
                             )
   is
   begin
      Raise_Exception_Always (E, Message);
   end Raise_Exception;

   procedure Reraise_Occurrence_No_Defer (
                                          X : Exception_Occurrence
                                         )
   is
      pragma Unreferenced (X);
   begin
      Warn_Not_Implemented ("Reraise_Occurrence_No_Defer");
   end Reraise_Occurrence_No_Defer;

   procedure Save_Occurrence (
                              Target : out Exception_Occurrence;
                              Source : Exception_Occurrence
                             )
   is
   begin
      Warn_Not_Implemented ("Save_Occurrence");
      Target := Source;
   end Save_Occurrence;

   procedure Warn_Not_Implemented (
                                   Name : String
                                  )
   is
      procedure C_Warn_Unimplemented_Function (
                                               Func : System.Address
                                              )
        with
          Import,
          Convention => C,
          External_Name => "warn_unimplemented_function";
      C_Name : String := Name & Character'Val (0);
   begin
      C_Warn_Unimplemented_Function (C_Name'Address);
   end Warn_Not_Implemented;

end Ada.Exceptions;
